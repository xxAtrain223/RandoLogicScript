using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.Utilities;

namespace RandoLogicScript.VisualStudio
{
    [ContentType(RlsContentType.Name)]
    [Export(typeof(ILanguageClient))]
    internal sealed class RlsLanguageClient : ILanguageClient, IDisposable
    {
        private readonly object processLock = new object();
        private readonly IRlsClientEnvironment environment;
        private readonly IRlsClientLog log;
        private readonly IRlsServerProcessFactory processFactory;
        private IRlsServerProcess serverProcess;
        private CancellationTokenRegistration serverCancellation;
        private CancellationToken serverCancellationToken;
        private bool serverInitialized;
        private bool disposed;

        public RlsLanguageClient()
            : this(new RlsClientEnvironment(), new RlsServerProcessFactory(), new RlsClientLog())
        {
        }

        internal RlsLanguageClient(
            IRlsClientEnvironment environment,
            IRlsServerProcessFactory processFactory,
            IRlsClientLog log)
        {
            this.environment = environment ?? throw new ArgumentNullException(nameof(environment));
            this.processFactory = processFactory ?? throw new ArgumentNullException(nameof(processFactory));
            this.log = log ?? throw new ArgumentNullException(nameof(log));
        }

        public string Name => "Rando Logic Script Language Server";

        public IEnumerable<string> ConfigurationSections => new[] { "randoLogicScript" };

        public object InitializationOptions => new
        {
            completion = new
            {
                sectionSnippetIndentation = "server",
            },
            semanticTokens = new
            {
                legend = new
                {
                    tokenTypes = new Dictionary<string, string>
                    {
                        ["function"] = "method name",
                        ["parameter"] = "parameter name",
                        ["enum"] = "cppValueType",
                        ["enumMember"] = "cppEnumerator",
                        ["property"] = "string",
                        ["variable"] = "local name",
                        ["operator"] = "operator",
                        ["rlsPropertyDeclaration"] = "string",
                    },
                    tokenModifiers = new Dictionary<string, string>
                    {
                        ["declaration"] = "rlsNoStyleModifier",
                        ["definition"] = "rlsNoStyleModifier",
                        ["readonly"] = "rlsNoStyleModifier",
                        ["defaultLibrary"] = "rlsNoStyleModifier",
                        ["deprecated"] = "rlsNoStyleModifier",
                    },
                },
            },
        };

        public IEnumerable<string> FilesToWatch => new[] { "**/*.rls", "**/rls.json" };

        public bool ShowNotificationOnInitializeFailed => true;

        public object MiddleLayer => null;

    #pragma warning disable CS0067
        public event AsyncEventHandler<EventArgs> StartAsync;

        public event AsyncEventHandler<EventArgs> StopAsync;
    #pragma warning restore CS0067

        public async Task<Connection> ActivateAsync(CancellationToken token)
        {
            await Task.Yield();
            token.ThrowIfCancellationRequested();

            StopOwnedServerProcess();

            lock (processLock)
            {
                if (disposed)
                {
                    throw new ObjectDisposedException(nameof(RlsLanguageClient));
                }
            }

            string serverPath = RlsServerConfiguration.ResolveServerPath(environment);

            if (!environment.FileExists(serverPath))
            {
                string message = $"RLS language server executable was not found at '{serverPath}'.";
                log.Error(message);
                throw new FileNotFoundException(message, serverPath);
            }

            ProcessStartInfo startInfo = RlsServerConfiguration.CreateStartInfo(serverPath);
            IRlsServerProcess process = processFactory.Create(startInfo);

            try
            {
                if (!process.Start())
                {
                    throw new InvalidOperationException("RLS language server process did not start.");
                }
            }
            catch (Exception exception)
            {
                process.Dispose();
                log.Error($"Failed to start '{serverPath}': {exception}");
                throw;
            }

            process.ErrorReceived += message =>
            {
                Debug.WriteLine($"[RLS language server] {message}");
                log.Warning(message);
            };
            process.Exited += OnServerProcessExited;
            process.BeginErrorReadLine();

            lock (processLock)
            {
                if (disposed)
                {
                    StopServerProcess(process);
                    throw new ObjectDisposedException(nameof(RlsLanguageClient));
                }

                serverProcess = process;
                serverCancellationToken = token;
                serverInitialized = false;
            }

            var cancellation = token.Register(() => StopOwnedServerProcess(process, false));
            Connection connection = null;
            lock (processLock)
            {
                if (ReferenceEquals(serverProcess, process) && !token.IsCancellationRequested)
                {
                    serverCancellation = cancellation;
                    connection = new Connection(process.StandardOutput, process.StandardInput);
                }
                else
                {
                    cancellation.Dispose();
                }
            }

            if (connection == null)
            {
                StopOwnedServerProcess(process);
                token.ThrowIfCancellationRequested();
                throw new InvalidOperationException("Language server process ownership was lost during startup.");
            }

            log.Information($"Started language server '{serverPath}'.");
            return connection;
        }

        public Task OnLoadedAsync()
        {
            return StartAsync?.InvokeAsync(this, EventArgs.Empty) ?? Task.CompletedTask;
        }

        public Task OnServerInitializedAsync()
        {
            lock (processLock)
            {
                serverInitialized = true;
            }

            log.Information("Language server initialized.");
            return Task.CompletedTask;
        }

        public Task<InitializationFailureContext> OnServerInitializeFailedAsync(
            ILanguageClientInitializationInfo initializationState)
        {
            string message = initializationState?.StatusMessage;
            if (string.IsNullOrWhiteSpace(message))
            {
                message = initializationState?.InitializationException?.Message
                    ?? "The RLS language server failed to initialize.";
            }

            log.Error(message);
            StopOwnedServerProcess();
            return Task.FromResult(new InitializationFailureContext { FailureMessage = message });
        }

        public void Dispose()
        {
            lock (processLock)
            {
                if (disposed)
                {
                    return;
                }

                disposed = true;
            }

            StopOwnedServerProcess();
        }

        private void OnServerProcessExited(object sender, EventArgs eventArgs)
        {
            var process = sender as IRlsServerProcess;
            bool unexpectedExit;
            int exitCode = -1;
            CancellationTokenRegistration cancellation;

            lock (processLock)
            {
                if (!ReferenceEquals(serverProcess, process))
                {
                    return;
                }

                try
                {
                    exitCode = process.ExitCode;
                }
                catch (InvalidOperationException)
                {
                }

                unexpectedExit = !disposed
                    && serverInitialized
                    && !serverCancellationToken.IsCancellationRequested;
                serverProcess = null;
                serverCancellationToken = default;
                serverInitialized = false;
                cancellation = serverCancellation;
                serverCancellation = default;
                process.Exited -= OnServerProcessExited;
            }

            cancellation.Dispose();
            process.Dispose();

            if (!unexpectedExit)
            {
                return;
            }

            log.Warning(
                $"Language server exited unexpectedly with code {exitCode}; Visual Studio will apply its bounded recovery policy.");
        }

        private void StopOwnedServerProcess(
            IRlsServerProcess expectedProcess = null,
            bool disposeCancellation = true)
        {
            IRlsServerProcess process;
            CancellationTokenRegistration cancellation;

            lock (processLock)
            {
                if (expectedProcess != null && !ReferenceEquals(serverProcess, expectedProcess))
                {
                    return;
                }

                process = serverProcess;
                serverProcess = null;
                serverCancellationToken = default;
                serverInitialized = false;
                cancellation = serverCancellation;
                serverCancellation = default;
            }

            if (disposeCancellation)
            {
                cancellation.Dispose();
            }

            StopServerProcess(process);
        }

        private static void StopServerProcess(IRlsServerProcess process)
        {
            if (process == null)
            {
                return;
            }

            try
            {
                if (!process.HasExited)
                {
                    process.Kill();
                }
            }
            catch (Exception exception) when (
                exception is InvalidOperationException || exception is System.ComponentModel.Win32Exception)
            {
            }
            finally
            {
                process.Dispose();
            }
        }
    }
}