const assert = require("assert");
const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawnSync } = require("child_process");

const grammarDirectory = __dirname;
const fixturePath = path.resolve(grammarDirectory, "..", "syntax-fixtures", "representative.rls");
const highlightsPath = path.join(grammarDirectory, "queries", "highlights.scm");
const treeSitter = process.platform === "win32" ? "tree-sitter.cmd" : "tree-sitter";

function runTreeSitter(arguments) {
  const result = spawnSync(treeSitter, arguments, {
    cwd: grammarDirectory,
    encoding: "utf8",
    shell: process.platform === "win32"
  });
  return {
    status: result.status,
    output: `${result.stdout || ""}${result.stderr || ""}`
  };
}

const fixture = fs.readFileSync(fixturePath, "utf8");
const incompleteMarker = "# Keep this incomplete source editable while typing.";
const validFixture = fixture.slice(0, fixture.indexOf(incompleteMarker));
const temporaryDirectory = fs.mkdtempSync(path.join(os.tmpdir(), "rls-tree-sitter-"));
const validFixturePath = path.join(temporaryDirectory, "representative-valid.rls");
fs.writeFileSync(validFixturePath, validFixture);

try {
  const validParse = runTreeSitter(["parse", validFixturePath]);
  assert.strictEqual(validParse.status, 0, validParse.output);
  assert(!validParse.output.includes("ERROR"), validParse.output);

  const incompleteParse = runTreeSitter(["parse", fixturePath]);
  assert.notStrictEqual(incompleteParse.status, 0, "Expected incomplete input to report a parse error.");
  assert(incompleteParse.output.includes("(ERROR"), incompleteParse.output);

  const highlights = runTreeSitter(["query", highlightsPath, validFixturePath]);
  assert.strictEqual(highlights.status, 0, highlights.output);
  ["comment", "string", "number", "boolean", "type", "function", "constant", "variable.parameter", "property", "operator"].forEach((capture) => {
    assert(highlights.output.includes(capture), `Expected @${capture} in highlight output.\n${highlights.output}`);
  });

  const folds = runTreeSitter(["query", "queries/folds.scm", validFixturePath]);
  assert.strictEqual(folds.status, 0, folds.output);
  assert(folds.output.includes("fold"), folds.output);

  const indents = runTreeSitter(["query", "queries/indents.scm", validFixturePath]);
  assert.strictEqual(indents.status, 0, indents.output);
  ["indent", "outdent"].forEach((capture) => {
    assert(indents.output.includes(capture), `Expected @${capture} in indent output.\n${indents.output}`);
  });
} finally {
  fs.unlinkSync(validFixturePath);
  fs.rmdirSync(temporaryDirectory);
}