using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;

using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.Utilities;

namespace RandoLogicScript.VisualStudio
{
    [ContentType(RlsContentType.Name)]
    [Export(typeof(ILanguageClient))]
    internal sealed class RlsLanguageClient : ILanguageClient, IDisposable
    {
        private const string LogSource = "Rando Logic Script";

        private readonly object processLock = new object();
        private Process serverProcess;
        private CancellationTokenRegistration serverCancellation;
        private CancellationToken serverCancellationToken;
        private bool serverInitialized;
        private bool disposed;

        public string Name => "Rando Logic Script Language Server";

        public IEnumerable<string> ConfigurationSections => new[] { "randoLogicScript" };

        public object InitializationOptions => new
        {
            completion = new
            {
                sectionSnippetIndentation = "server",
            },
        };

        public IEnumerable<string> FilesToWatch => new[] { "**/*.rls", "**/rls.json" };

        public bool ShowNotificationOnInitializeFailed => true;

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

            string extensionDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            string bundledServerPath = Path.Combine(extensionDirectory, "Server", "rls_language_server.exe");
            string configuredServerPath = Environment.GetEnvironmentVariable("RLS_LANGUAGE_SERVER_PATH");
            string serverPath = string.IsNullOrWhiteSpace(configuredServerPath)
                ? bundledServerPath
                : configuredServerPath;

            if (!File.Exists(serverPath))
            {
                string message = $"RLS language server executable was not found at '{serverPath}'.";
                ActivityLog.TryLogError(LogSource, message);
                throw new FileNotFoundException(message, serverPath);
            }

            var startInfo = new ProcessStartInfo
            {
                FileName = serverPath,
                WorkingDirectory = Path.GetDirectoryName(serverPath),
                RedirectStandardInput = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true,
            };

            var process = new Process
            {
                EnableRaisingEvents = true,
                StartInfo = startInfo,
            };

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
                ActivityLog.TryLogError(LogSource, $"Failed to start '{serverPath}': {exception}");
                throw;
            }

            process.ErrorDataReceived += (_, eventArgs) =>
            {
                if (!string.IsNullOrEmpty(eventArgs.Data))
                {
                    Debug.WriteLine($"[RLS language server] {eventArgs.Data}");
                    ActivityLog.TryLogWarning(LogSource, eventArgs.Data);
                }
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
            lock (processLock)
            {
                if (ReferenceEquals(serverProcess, process))
                {
                    serverCancellation = cancellation;
                }
                else
                {
                    cancellation.Dispose();
                }
            }

            ActivityLog.TryLogInformation(LogSource, $"Started language server '{serverPath}'.");
            return new Connection(process.StandardOutput.BaseStream, process.StandardInput.BaseStream);
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

            ActivityLog.TryLogInformation(LogSource, "Language server initialized.");
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

            ActivityLog.TryLogError(LogSource, message);
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
            var process = sender as Process;
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

            ActivityLog.TryLogWarning(
                LogSource,
                $"Language server exited unexpectedly with code {exitCode}; Visual Studio will apply its bounded recovery policy.");
        }

        private void StopOwnedServerProcess(
            Process expectedProcess = null,
            bool disposeCancellation = true)
        {
            Process process;
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

        private static void StopServerProcess(Process process)
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