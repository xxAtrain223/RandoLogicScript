using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;

using Microsoft.VisualStudio.LanguageServer.Client;

using Xunit;

namespace RandoLogicScript.VisualStudio.Tests
{
    public sealed class RlsLanguageClientTests
    {
        [Fact]
        public void ResolveServerPathUsesBundledServerByDefault()
        {
            var environment = new FakeEnvironment { ExtensionDirectory = @"C:\extension" };

            string path = RlsServerConfiguration.ResolveServerPath(environment);

            Assert.Equal(@"C:\extension\Server\rls_language_server.exe", path);
        }

        [Fact]
        public void ResolveServerPathPrefersDevelopmentOverride()
        {
            var environment = new FakeEnvironment
            {
                ExtensionDirectory = @"C:\extension",
                ServerOverride = @"D:\build\rls_language_server.exe",
            };

            string path = RlsServerConfiguration.ResolveServerPath(environment);

            Assert.Equal(environment.ServerOverride, path);
        }

        [Fact]
        public void CreateStartInfoConfiguresStdioWithoutShell()
        {
            ProcessStartInfo startInfo = RlsServerConfiguration.CreateStartInfo(
                @"D:\build\rls_language_server.exe");

            Assert.Equal(@"D:\build\rls_language_server.exe", startInfo.FileName);
            Assert.Equal(@"D:\build", startInfo.WorkingDirectory);
            Assert.True(startInfo.RedirectStandardInput);
            Assert.True(startInfo.RedirectStandardOutput);
            Assert.True(startInfo.RedirectStandardError);
            Assert.False(startInfo.UseShellExecute);
            Assert.True(startInfo.CreateNoWindow);
        }

        [Fact]
        public void AdvertisesExpectedOptionsAndWatchPatterns()
        {
            using (var client = CreateClient(out _, out _, out _))
            {
                Assert.Equal(new[] { "randoLogicScript" }, client.ConfigurationSections);
                Assert.Equal(new[] { "**/*.rls", "**/rls.json" }, client.FilesToWatch);
                Assert.Equal(
                    "server",
                    ReadProperty(ReadProperty(client.InitializationOptions, "completion"), "sectionSnippetIndentation"));
                object legend = ReadProperty(
                    ReadProperty(ReadProperty(client.InitializationOptions, "semanticTokens"), "legend"),
                    "tokenTypes");
                var tokenTypes = Assert.IsAssignableFrom<IDictionary<string, string>>(legend);
                Assert.Equal("method name", tokenTypes["function"]);
                Assert.Equal("cppValueType", tokenTypes["enum"]);
                Assert.Equal("cppEnumerator", tokenTypes["enumMember"]);
                Assert.Equal("string", tokenTypes["property"]);
                Assert.Equal("string", tokenTypes["rlsPropertyDeclaration"]);
                var tokenModifiers = Assert.IsAssignableFrom<IDictionary<string, string>>(
                    ReadProperty(
                        ReadProperty(ReadProperty(client.InitializationOptions, "semanticTokens"), "legend"),
                        "tokenModifiers"));
                Assert.All(tokenModifiers.Values, value =>
                    Assert.Equal("rlsNoStyleModifier", value));
                Assert.True(client.ShowNotificationOnInitializeFailed);
                Assert.Null(client.MiddleLayer);
            }
        }

        [Fact]
        public async Task ActivateUsesOverrideAndStartsErrorReading()
        {
            using (var client = CreateClient(out var environment, out var factory, out var log))
            {
                environment.ServerOverride = @"D:\build\rls_language_server.exe";
                await client.ActivateAsync(CancellationToken.None);

                FakeProcess process = Assert.Single(factory.Processes);
                Assert.Equal(environment.ServerOverride, factory.StartInfos.Single().FileName);
                Assert.True(process.StartCalled);
                Assert.True(process.BeginErrorReadLineCalled);
                Assert.Contains(log.InformationMessages, message => message.Contains(environment.ServerOverride));
            }
        }

        [Fact]
        public async Task ActivateRejectsMissingServerBeforeCreatingProcess()
        {
            using (var client = CreateClient(out var environment, out var factory, out var log))
            {
                environment.Exists = false;

                await Assert.ThrowsAsync<FileNotFoundException>(() =>
                    client.ActivateAsync(CancellationToken.None));

                Assert.Empty(factory.Processes);
                Assert.Single(log.ErrorMessages);
            }
        }

        [Fact]
        public async Task ActivationCancellationKillsAndDisposesOwnedProcess()
        {
            using (var cancellation = new CancellationTokenSource())
            using (var client = CreateClient(out _, out var factory, out _))
            {
                await client.ActivateAsync(cancellation.Token);
                FakeProcess process = factory.Processes.Single();

                cancellation.Cancel();

                Assert.True(process.KillCalled);
                Assert.True(process.DisposeCalled);
            }
        }

        [Fact]
        public async Task SecondActivationStopsPreviouslyOwnedProcess()
        {
            using (var client = CreateClient(out _, out var factory, out _))
            {
                await client.ActivateAsync(CancellationToken.None);
                FakeProcess first = factory.Processes.Single();

                await client.ActivateAsync(CancellationToken.None);

                Assert.True(first.KillCalled);
                Assert.True(first.DisposeCalled);
                Assert.Equal(2, factory.Processes.Count);
            }
        }

        [Fact]
        public async Task InitializedUnexpectedExitLogsRecoveryAndReleasesProcess()
        {
            using (var client = CreateClient(out _, out var factory, out var log))
            {
                await client.ActivateAsync(CancellationToken.None);
                await client.OnServerInitializedAsync();
                FakeProcess process = factory.Processes.Single();
                process.ExitCodeValue = 23;

                process.RaiseExited();

                Assert.True(process.DisposeCalled);
                Assert.Contains(log.WarningMessages, message =>
                    message.Contains("code 23") && message.Contains("bounded recovery"));
            }
        }

        [Fact]
        public async Task StderrIsLoggedWithoutAffectingConnectionStreams()
        {
            using (var client = CreateClient(out _, out var factory, out var log))
            {
                Connection connection = await client.ActivateAsync(CancellationToken.None);
                FakeProcess process = factory.Processes.Single();

                process.RaiseError("server warning");

                Assert.Contains("server warning", log.WarningMessages);
                Assert.Same(process.StandardOutput, ReadProperty(connection, "Reader"));
                Assert.Same(process.StandardInput, ReadProperty(connection, "Writer"));
            }
        }

        [Fact]
        public async Task InitializationFailureStopsProcessAndReturnsMessage()
        {
            using (var client = CreateClient(out _, out var factory, out var log))
            {
                await client.ActivateAsync(CancellationToken.None);
                var initialization = new FakeInitializationInfo { StatusMessage = "bad initialize" };

                InitializationFailureContext context =
                    await client.OnServerInitializeFailedAsync(initialization);

                Assert.Equal("bad initialize", context.FailureMessage);
                Assert.True(factory.Processes.Single().KillCalled);
                Assert.Contains("bad initialize", log.ErrorMessages);
            }
        }

        [Fact]
        public async Task DisposedClientRejectsActivation()
        {
            var client = CreateClient(out _, out _, out _);
            client.Dispose();

            await Assert.ThrowsAsync<ObjectDisposedException>(() =>
                client.ActivateAsync(CancellationToken.None));
        }

        private static RlsLanguageClient CreateClient(
            out FakeEnvironment environment,
            out FakeProcessFactory factory,
            out FakeLog log)
        {
            environment = new FakeEnvironment
            {
                ExtensionDirectory = @"C:\extension",
                Exists = true,
            };
            factory = new FakeProcessFactory();
            log = new FakeLog();
            return new RlsLanguageClient(environment, factory, log);
        }

        private static object ReadProperty(object value, string name)
        {
            return value.GetType()
                .GetProperty(name, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)
                ?.GetValue(value);
        }

        private sealed class FakeEnvironment : IRlsClientEnvironment
        {
            public string ExtensionDirectory { get; set; }

            public string ServerOverride { get; set; }

            public bool Exists { get; set; }

            public bool FileExists(string path) => Exists;
        }

        private sealed class FakeLog : IRlsClientLog
        {
            public List<string> ErrorMessages { get; } = new List<string>();

            public List<string> InformationMessages { get; } = new List<string>();

            public List<string> WarningMessages { get; } = new List<string>();

            public void Error(string message) => ErrorMessages.Add(message);

            public void Information(string message) => InformationMessages.Add(message);

            public void Warning(string message) => WarningMessages.Add(message);
        }

        private sealed class FakeProcessFactory : IRlsServerProcessFactory
        {
            public List<FakeProcess> Processes { get; } = new List<FakeProcess>();

            public List<ProcessStartInfo> StartInfos { get; } = new List<ProcessStartInfo>();

            public IRlsServerProcess Create(ProcessStartInfo startInfo)
            {
                var process = new FakeProcess();
                Processes.Add(process);
                StartInfos.Add(startInfo);
                return process;
            }
        }

        private sealed class FakeProcess : IRlsServerProcess
        {
            public event EventHandler Exited;

            public event Action<string> ErrorReceived;

            public Stream StandardInput { get; } = new MemoryStream();

            public Stream StandardOutput { get; } = new MemoryStream();

            public bool HasExited { get; set; }

            public int ExitCode => ExitCodeValue;

            public int ExitCodeValue { get; set; }

            public bool StartCalled { get; private set; }

            public bool BeginErrorReadLineCalled { get; private set; }

            public bool KillCalled { get; private set; }

            public bool DisposeCalled { get; private set; }

            public bool Start()
            {
                StartCalled = true;
                return true;
            }

            public void BeginErrorReadLine() => BeginErrorReadLineCalled = true;

            public void Kill()
            {
                KillCalled = true;
                HasExited = true;
            }

            public void Dispose() => DisposeCalled = true;

            public void RaiseExited()
            {
                HasExited = true;
                Exited?.Invoke(this, EventArgs.Empty);
            }

            public void RaiseError(string message) => ErrorReceived?.Invoke(message);
        }

        private sealed class FakeInitializationInfo : ILanguageClientInitializationInfo
        {
            public Exception InitializationException { get; set; }

            public bool IsInitialized { get; set; }

            public InitializationStatus Status { get; set; }

            public string StatusMessage { get; set; }
        }
    }
}