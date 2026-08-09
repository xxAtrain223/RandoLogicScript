const assert = require("assert");
const fs = require("fs");
const path = require("path");
const oniguruma = require("vscode-oniguruma");
const textmate = require("vscode-textmate");

const textmateDirectory = __dirname;
const repositoryDirectory = path.resolve(textmateDirectory, "..", "..");
const grammarPath = path.join(repositoryDirectory, "editors", "vscode", "syntaxes", "rls.tmLanguage.json");
const fixturePath = path.join(repositoryDirectory, "tooling", "syntax-fixtures", "representative.rls");
const snapshotPath = path.join(textmateDirectory, "snapshots", "representative.scopes.json");

function loadOniguruma() {
  const wasmPath = require.resolve("vscode-oniguruma/release/onig.wasm");
  return oniguruma.loadWASM(fs.readFileSync(wasmPath).buffer);
}

function serializeScopes(grammar, source) {
  let ruleStack = textmate.INITIAL;
  return source.split(/\r?\n/).map((line, index) => {
    const tokenizedLine = grammar.tokenizeLine(line, ruleStack);
    ruleStack = tokenizedLine.ruleStack;
    return {
      line: index + 1,
      text: line,
      tokens: tokenizedLine.tokens.map((token) => ({
        start: token.startIndex,
        end: token.endIndex,
        scopes: token.scopes
      }))
    };
  });
}

async function main() {
  await loadOniguruma();
  const registry = new textmate.Registry({
    onigLib: Promise.resolve({
      createOnigScanner: (sources) => new oniguruma.OnigScanner(sources),
      createOnigString: (value) => new oniguruma.OnigString(value)
    }),
    loadGrammar: (scopeName) => {
      if (scopeName !== "source.rls") {
        return null;
      }
      return JSON.parse(fs.readFileSync(grammarPath, "utf8"));
    }
  });

  const grammar = await registry.loadGrammar("source.rls");
  assert(grammar, "Expected the source.rls grammar to load.");
  const snapshot = serializeScopes(grammar, fs.readFileSync(fixturePath, "utf8"));

  if (process.argv.includes("--update")) {
    fs.mkdirSync(path.dirname(snapshotPath), { recursive: true });
    fs.writeFileSync(snapshotPath, `${JSON.stringify(snapshot, null, 2)}\n`);
    return;
  }

  assert(fs.existsSync(snapshotPath), "Scope snapshot is missing. Run npm run update-snapshots.");
  const expected = JSON.parse(fs.readFileSync(snapshotPath, "utf8"));
  assert.deepStrictEqual(snapshot, expected, "TextMate scopes changed. Review the diff and run npm run update-snapshots if intentional.");
}

main().catch((error) => {
  console.error(error.stack || error.message);
  process.exitCode = 1;
});