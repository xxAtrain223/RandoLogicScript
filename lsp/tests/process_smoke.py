#!/usr/bin/env python3

import argparse
import json
import queue
import subprocess
import tempfile
import threading
from pathlib import Path


class ProtocolError(RuntimeError):
    pass


def encode_message(message: dict) -> bytes:
    payload = json.dumps(message, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    return f"Content-Length: {len(payload)}\r\n\r\n".encode("ascii") + payload


def read_message(stream) -> dict | None:
    headers: dict[str, str] = {}
    while True:
        line = stream.readline()
        if line == b"":
            return None
        if not line.endswith(b"\r\n"):
            raise ProtocolError("stdout contained a malformed protocol header")
        if line == b"\r\n":
            break
        name, separator, value = line.decode("ascii").partition(":")
        if not separator:
            raise ProtocolError("stdout contained a malformed protocol header")
        headers[name.lower()] = value.strip()

    try:
        length = int(headers["content-length"])
    except (KeyError, ValueError) as error:
        raise ProtocolError("stdout frame omitted a valid Content-Length") from error

    payload = stream.read(length)
    if len(payload) != length:
        raise ProtocolError("stdout frame ended before its payload")
    return json.loads(payload.decode("utf-8"))


def output_reader(stream, messages: queue.Queue) -> None:
    try:
        while True:
            message = read_message(stream)
            if message is None:
                messages.put(None)
                return
            messages.put(message)
    except BaseException as error:
        messages.put(error)


def send(process: subprocess.Popen, message: dict) -> None:
    assert process.stdin is not None
    process.stdin.write(encode_message(message))
    process.stdin.flush()


def receive_matching(messages: queue.Queue, predicate, description: str) -> dict:
    deferred: list[dict] = []
    try:
        while True:
            try:
                message = messages.get(timeout=10)
            except queue.Empty as error:
                raise ProtocolError(f"timed out waiting for {description}") from error
            if isinstance(message, BaseException):
                raise message
            if message is None:
                raise ProtocolError(f"server exited before {description}")
            if predicate(message):
                return message
            deferred.append(message)
    finally:
        for message in deferred:
            messages.put(message)


def run_smoke(server: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="rls lsp smoke ") as temporary:
        root = Path(temporary)
        source = root / "logic file.rls"
        manifest = root / "rls.json"
        source.write_text("define disk(): true\n", encoding="utf-8")
        manifest.write_text(
            json.dumps({"version": 1, "sources": [source.name]}), encoding="utf-8"
        )

        root_uri = root.as_uri()
        source_uri = source.as_uri()
        if "%20" not in root_uri or "%20" not in source_uri:
            raise ProtocolError("URI fixture did not exercise escaped paths")

        process = subprocess.Popen(
            [str(server)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        assert process.stdout is not None
        assert process.stderr is not None
        messages: queue.Queue = queue.Queue()
        reader = threading.Thread(
            target=output_reader, args=(process.stdout, messages), daemon=True
        )
        reader.start()

        try:
            send(
                process,
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "initialize",
                    "params": {
                        "workspaceFolders": [{"uri": root_uri, "name": "smoke"}]
                    },
                },
            )
            initialize = receive_matching(
                messages, lambda message: message.get("id") == 1, "initialize response"
            )
            capabilities = initialize["result"]["capabilities"]
            if capabilities["textDocumentSync"]["change"] != 1:
                raise ProtocolError("server did not advertise full document synchronization")
            if not capabilities["workspace"]["workspaceFolders"]["supported"]:
                raise ProtocolError("server did not advertise workspace folder support")
            if capabilities.get("signatureHelpProvider", {}).get(
                "triggerCharacters"
            ) != ["(", ","]:
                raise ProtocolError("server did not advertise signature help triggers")
            if capabilities.get("hoverProvider") is not True:
                raise ProtocolError("server did not advertise hover support")
            semantic_tokens = capabilities.get("semanticTokensProvider", {})
            if semantic_tokens.get("legend", {}).get("tokenTypes") != [
                "function",
                "parameter",
                "enum",
                "enumMember",
                "property",
                "variable",
                "operator",
            ] or semantic_tokens.get("full") is not True:
                raise ProtocolError("server did not advertise semantic token support")

            send(process, {"jsonrpc": "2.0", "method": "initialized", "params": {}})
            send(
                process,
                {
                    "jsonrpc": "2.0",
                    "method": "textDocument/didOpen",
                    "params": {
                        "textDocument": {
                            "uri": source_uri,
                            "languageId": "rls",
                            "version": 1,
                            "text": "define broken(): missing\n",
                        }
                    },
                },
            )
            diagnostic_message = receive_matching(
                messages,
                lambda message: message.get("method")
                == "textDocument/publishDiagnostics"
                and message.get("params", {}).get("uri") == source_uri
                and message.get("params", {}).get("diagnostics"),
                "live diagnostic notification",
            )
            diagnostic = next(
                item
                for item in diagnostic_message["params"]["diagnostics"]
                if item.get("code") == "RLS-T006"
            )
            if diagnostic["data"] != {
                "version": 1,
                "actionKind": "rls.declareSymbol",
                "arguments": ["missing"],
            }:
                raise ProtocolError("structured diagnostic data was not preserved")

            send(
                process,
                {
                    "jsonrpc": "2.0",
                    "method": "textDocument/didChange",
                    "params": {
                        "textDocument": {"uri": source_uri, "version": 2},
                        "contentChanges": [{"text": ""}],
                    },
                },
            )
            receive_matching(
                messages,
                lambda message: message.get("method")
                == "textDocument/publishDiagnostics"
                and message.get("params", {}).get("uri") == source_uri
                and not message.get("params", {}).get("diagnostics"),
                "diagnostic clear notification",
            )

            signature_source = (
                "extern define target(value: Bool = true) -> Bool\n"
                "define use(): target(f"
            )
            send(
                process,
                {
                    "jsonrpc": "2.0",
                    "method": "textDocument/didChange",
                    "params": {
                        "textDocument": {"uri": source_uri, "version": 3},
                        "contentChanges": [{"text": signature_source}],
                    },
                },
            )
            send(
                process,
                {
                    "jsonrpc": "2.0",
                    "id": 2,
                    "method": "textDocument/signatureHelp",
                    "params": {
                        "textDocument": {"uri": source_uri},
                        "position": {"line": 1, "character": len("define use(): target(f")},
                    },
                },
            )
            signature = receive_matching(
                messages, lambda message: message.get("id") == 2,
                "signature help response",
            )["result"]
            if signature["activeParameter"] != 0 or signature["signatures"][0][
                "label"
            ] != "extern target(value: Bool = true) -> Bool":
                raise ProtocolError("signature help response was incomplete")

            send(
                process,
                {
                    "jsonrpc": "2.0",
                    "id": 3,
                    "method": "textDocument/hover",
                    "params": {
                        "textDocument": {"uri": source_uri},
                        "position": {"line": 1, "character": len("define use(): tar")},
                    },
                },
            )
            hover = receive_matching(
                messages, lambda message: message.get("id") == 3,
                "hover response",
            )["result"]
            if "extern target(value: Bool = true) -> Bool" not in hover[
                "contents"
            ]["value"]:
                raise ProtocolError("hover response omitted callable presentation")

            send(
                process,
                {
                    "jsonrpc": "2.0",
                    "id": 4,
                    "method": "textDocument/semanticTokens/full",
                    "params": {"textDocument": {"uri": source_uri}},
                },
            )
            token_data = receive_matching(
                messages, lambda message: message.get("id") == 4,
                "semantic token response",
            )["result"]["data"]
            if not token_data or len(token_data) % 5 != 0:
                raise ProtocolError("semantic token response was not delta encoded")

            send(process, {"jsonrpc": "2.0", "id": 5, "method": "shutdown"})
            receive_matching(
                messages, lambda message: message.get("id") == 5, "shutdown response"
            )
            send(process, {"jsonrpc": "2.0", "method": "exit"})
            assert process.stdin is not None
            process.stdin.close()
            exit_code = process.wait(timeout=10)
            reader.join(timeout=10)
            stderr = process.stderr.read().decode("utf-8", errors="replace")
            if exit_code != 0:
                raise ProtocolError(f"server exited with {exit_code}: {stderr}")
            if stderr:
                raise ProtocolError(f"server wrote to stderr during clean smoke test: {stderr}")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", type=Path, required=True)
    arguments = parser.parse_args()
    run_smoke(arguments.server.resolve())


if __name__ == "__main__":
    main()