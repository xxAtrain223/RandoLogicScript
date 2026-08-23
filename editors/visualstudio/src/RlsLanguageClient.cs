using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;

using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.Utilities;

namespace RandoLogicScript.VisualStudio
{
    [ContentType(RlsContentType.Name)]
    [Export(typeof(ILanguageClient))]
    internal sealed class RlsLanguageClient : ILanguageClient
    {
        private Process serverProcess;

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

            string extensionDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            string bundledServerPath = Path.Combine(extensionDirectory, "Server", "rls_language_server.exe");
            string configuredServerPath = Environment.GetEnvironmentVariable("RLS_LANGUAGE_SERVER_PATH");
            string serverPath = string.IsNullOrWhiteSpace(configuredServerPath)
                ? bundledServerPath
                : configuredServerPath;

            if (!File.Exists(serverPath))
            {
                throw new FileNotFoundException("RLS language server executable was not found.", serverPath);
            }

            var startInfo = new ProcessStartInfo
            {
                FileName = serverPath,
                WorkingDirectory = extensionDirectory,
                RedirectStandardInput = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true,
            };

            var process = new Process { StartInfo = startInfo };
            if (!process.Start())
            {
                process.Dispose();
                throw new InvalidOperationException("RLS language server process did not start.");
            }

            process.ErrorDataReceived += (_, eventArgs) =>
            {
                if (!string.IsNullOrEmpty(eventArgs.Data))
                {
                    Debug.WriteLine($"[RLS language server] {eventArgs.Data}");
                }
            };
            process.BeginErrorReadLine();

            serverProcess = process;
            token.Register(() => StopServerProcess(process));
            return new Connection(process.StandardOutput.BaseStream, process.StandardInput.BaseStream);
        }

        public Task OnLoadedAsync()
        {
            return StartAsync?.InvokeAsync(this, EventArgs.Empty) ?? Task.CompletedTask;
        }

        public Task OnServerInitializedAsync()
        {
            return Task.CompletedTask;
        }

        public Task<InitializationFailureContext> OnServerInitializeFailedAsync(
            ILanguageClientInitializationInfo initializationState)
        {
            StopServerProcess(serverProcess);
            return Task.FromResult(new InitializationFailureContext());
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
            catch (InvalidOperationException)
            {
            }
            finally
            {
                process.Dispose();
            }
        }
    }
}