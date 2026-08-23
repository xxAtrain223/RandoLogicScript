# Releasing Editor Extensions

Tagged releases produce four VSIX files:

| Package | Runner | Native server |
| --- | --- | --- |
| VS Code `win32-x64` | `windows-latest` | Statically linked Release executable |
| VS Code `linux-x64` | `ubuntu-22.04` | Release executable |
| VS Code `darwin-x64` | `macos-15-intel` | Release executable targeting macOS 15 or later |
| Visual Studio x64 | `windows-latest` | Statically linked Release executable |

Native executables are build artifacts. Do not commit them to Git. CI stores validated
VSIX files for seven days, and tagged releases store them permanently as GitHub Release
assets. Each VSIX contains exactly one compatible native server. The Visual Studio
package is named `rando-logic-script-v<version>-visualstudio-x64.vsix` and targets
amd64 Visual Studio `[17.0,18.0)`. Visual Studio 2026 accepts this compatible package.

## Release Process

1. Update the extension version without creating a tag:

   ```sh
   cd editors/vscode
   npm version <version> --no-git-tag-version
   ```

2. Update the `Version` in
   `editors/visualstudio/src/source.extension.vsixmanifest` to the same value.
3. Update `editors/vscode/CHANGELOG.md` and `editors/visualstudio/CHANGELOG.md`, then
   commit the version and release notes.
4. Complete `editors/visualstudio/MANUAL-TESTING.md` for the release candidate.
5. Wait for CI to pass. CI compiles and tests the project, packages each targeted VSIX,
   validates its manifest and native executable, and stores the validated deliverables.
6. Create and push a matching tag:

   ```sh
   git tag v0.1.0
   git push origin v0.1.0
   ```

7. The release workflow repeats clean Release builds, runs the expanded language-server
   process smoke test and C# unit tests, validates all four VSIX files, records signed
   GitHub build provenance, and creates the GitHub Release.

The workflow rejects a tag whose version does not exactly match
`editors/vscode/package.json`. The Visual Studio validator also rejects a manifest
version that differs from the VS Code package version.

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

The Visual Studio package is verified the same way:

```sh
gh attestation verify \
   rando-logic-script-v0.1.0-visualstudio-x64.vsix \
   --repo xxAtrain223/RandoLogicScript
```

## VS Code Marketplace Publication

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

After all packages pass validation, the tag workflow creates the GitHub Release and
enters the `marketplace-production` environment. It downloads only artifacts named
`rando-logic-script-vscode-*`, verifies there are exactly three, signs in through
`azure/login`, and publishes them with:

```sh
npx --no-install vsce publish --azure-credential --packagePath <validated-target.vsix>
```

Publish every target for a version. Configure required reviewers on the GitHub
environment if Marketplace publication should require manual approval.

## Visual Studio Marketplace Publication

The Visual Studio VSIX is attached to the GitHub Release and attested automatically,
but its Marketplace listing is published manually. Do not pass it to `vsce`; `vsce`
publishes VS Code extension packages only.

For the first listing or an update:

1. Download `rando-logic-script-v<version>-visualstudio-x64.vsix` from the GitHub
   Release and verify its attestation.
2. Sign in to the Visual Studio Marketplace publisher portal for `xxAtrain223`.
3. Create or update the Visual Studio extension listing and upload the validated VSIX.
4. Use `editors/visualstudio/README.md` and `CHANGELOG.md` for listing details and
   release notes.
5. Confirm the listing states Windows x64, Visual Studio 2022-compatible, tested on
   Visual Studio 2026, and excludes Visual Studio 2019 and ARM64.
6. Install the Marketplace-delivered package and repeat the install/activation/uninstall
   portion of `editors/visualstudio/MANUAL-TESTING.md`.
