# Releasing the VS Code Extension

The extension is released as separate VSIX files for these targets:

| VS Code target | Runner | Native server |
| --- | --- | --- |
| `win32-x64` | `windows-latest` | Statically linked Release executable |
| `linux-x64` | `ubuntu-22.04` | Release executable |
| `darwin-x64` | `macos-15-intel`, Xcode 26.3 | Release executable targeting macOS 15 or later |

Native executables are build artifacts. Do not commit them to Git. CI stores validated
VSIX files for seven days, and tagged releases store them permanently as GitHub Release
assets. Each targeted VSIX contains exactly one compatible native server.

## Release Process

1. Update the extension version without creating a tag:

   ```sh
   cd editors/vscode
   npm version <version> --no-git-tag-version
   ```

2. Update `editors/vscode/CHANGELOG.md`, then commit the version and release notes.
3. Wait for CI to pass. CI compiles and tests the project, packages each targeted VSIX,
   validates its manifest and native executable, and stores the validated deliverables.
4. Create and push a matching tag:

   ```sh
   git tag v0.1.0
   git push origin v0.1.0
   ```

5. The release workflow repeats the clean Release builds, runs the language-server
    process smoke test, validates each targeted VSIX, records signed GitHub build
    provenance, and creates the GitHub Release.

The workflow rejects a tag whose version does not exactly match
`editors/vscode/package.json`.

## Verifying Build Provenance

Each released VSIX has a GitHub artifact attestation that binds its SHA-256 digest to
this repository, the release workflow, the source commit, and the GitHub Actions build
identity. After downloading a VSIX, verify it online with GitHub CLI:

```sh
gh attestation verify \
   rando-logic-script-v0.1.0-linux-x64.vsix \
   --repo xxAtrain223/RandoLogicScript
```

Use `--format json` to inspect the complete provenance statement:

```sh
gh attestation verify \
   rando-logic-script-v0.1.0-linux-x64.vsix \
   --repo xxAtrain223/RandoLogicScript \
   --format json
```

Verification fails if the VSIX has been modified or if its attestation was not issued
for this repository. Attestation proves origin and integrity; it does not by itself
claim that an independent rebuild will be byte-for-byte identical.

## Marketplace Publication

Marketplace publishing uses Microsoft Entra workload identity federation. GitHub
Actions exchanges its short-lived OIDC token for an Azure credential, and `vsce`
uses that credential to publish. No personal access token or client secret is stored.

### One-Time Configuration

1. Create a user-assigned managed identity in Azure and grant it the Reader role on
   the subscription used by the release workflow.
2. Add a federated identity credential for this repository and the
   `marketplace-production` GitHub environment. Its subject is:

   ```text
   repo:xxAtrain223/RandoLogicScript:environment:marketplace-production
   ```

   Use `api://AzureADTokenExchange` as the audience.
3. Run `azure/login` with the federated identity, then retrieve its Azure DevOps
   profile resource ID with Azure CLI:

   ```sh
   az rest \
     --url https://app.vssps.visualstudio.com/_apis/profile/profiles/me \
     --resource 499b84ac-1321-427f-aa17-267ca6975798 \
     --query id \
     --output tsv
   ```

4. In the Visual Studio Marketplace publisher management page, add that resource ID
   to publisher `xxAtrain223` with the Contributor role.
5. Create a protected GitHub environment named `marketplace-production`. Add these
   environment secrets:

   - `AZURE_CLIENT_ID`: managed identity client ID
   - `AZURE_TENANT_ID`: Microsoft Entra tenant ID
   - `AZURE_SUBSCRIPTION_ID`: Azure subscription ID

The identifiers select the federated identity; none is a password. Restricting them to
the protected environment keeps Marketplace publication separate from pull-request and
ordinary CI jobs.

### Automated Publication

After all targeted packages pass validation, the tag workflow creates the GitHub
Release and enters the `marketplace-production` environment. It signs in through
`azure/login`, then publishes every validated platform package with:

```sh
npx --no-install vsce publish --azure-credential --packagePath <validated-target.vsix>
```

Publish every target for a version. Configure required reviewers on the GitHub
environment if Marketplace publication should require manual approval.