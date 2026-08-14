import * as assert from 'node:assert/strict';

import * as vscode from 'vscode';

async function waitForDiagnostic(
  uri: vscode.Uri,
  code: string,
): Promise<vscode.Diagnostic> {
  const deadline = Date.now() + 10_000;
  while (Date.now() < deadline) {
    const diagnostic = vscode.languages
      .getDiagnostics(uri)
      .find((candidate) => candidate.code === code);
    if (diagnostic) {
      return diagnostic;
    }
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  throw new Error(`Timed out waiting for diagnostic ${code}.`);
}

export async function runLanguageClientTest(): Promise<void> {
  const extension = vscode.extensions.getExtension(
    'xxAtrain223.rando-logic-script',
  );
  assert.ok(extension, 'extension is available in the development host');

  const workspace = vscode.workspace.workspaceFolders?.[0];
  assert.ok(workspace, 'test fixture opened as a workspace folder');
  const documentUri = vscode.Uri.joinPath(workspace.uri, 'diagnostic.rls');
  await vscode.workspace.openTextDocument(documentUri);
  const diagnostic = await waitForDiagnostic(documentUri, 'RLS-T006');
  assert.equal(extension.isActive, true, 'opening an RLS document activates the extension');
  assert.equal(diagnostic.severity, vscode.DiagnosticSeverity.Error);
  assert.match(diagnostic.message, /unknown identifier 'missing'/);

  await vscode.commands.executeCommand('randoLogicScript.restartServer');
  const restartedDiagnostic = await waitForDiagnostic(documentUri, 'RLS-T006');
  assert.equal(restartedDiagnostic.source, 'rls');
}