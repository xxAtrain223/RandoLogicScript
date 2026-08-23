using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;

using Microsoft.VisualStudio.Shell;

namespace RandoLogicScript.VisualStudio
{
    internal interface IRlsClientEnvironment
    {
        string ExtensionDirectory { get; }

        string ServerOverride { get; }

        bool FileExists(string path);
    }

    internal sealed class RlsClientEnvironment : IRlsClientEnvironment
    {
        public string ExtensionDirectory =>
            Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);

        public string ServerOverride =>
            Environment.GetEnvironmentVariable("RLS_LANGUAGE_SERVER_PATH");

        public bool FileExists(string path) => File.Exists(path);
    }

    internal interface IRlsClientLog
    {
        void Error(string message);

        void Information(string message);

        void Warning(string message);
    }

    internal sealed class RlsClientLog : IRlsClientLog
    {
        private const string Source = "Rando Logic Script";

        public void Error(string message) => ActivityLog.TryLogError(Source, message);

        public void Information(string message) => ActivityLog.TryLogInformation(Source, message);

        public void Warning(string message) => ActivityLog.TryLogWarning(Source, message);
    }

    internal interface IRlsServerProcess : IDisposable
    {
        event EventHandler Exited;

        event Action<string> ErrorReceived;

        Stream StandardInput { get; }

        Stream StandardOutput { get; }

        bool HasExited { get; }

        int ExitCode { get; }

        bool Start();

        void BeginErrorReadLine();

        void Kill();
    }

    internal interface IRlsServerProcessFactory
    {
        IRlsServerProcess Create(ProcessStartInfo startInfo);
    }

    internal sealed class RlsServerProcessFactory : IRlsServerProcessFactory
    {
        public IRlsServerProcess Create(ProcessStartInfo startInfo) =>
            new RlsServerProcess(startInfo);
    }

    internal sealed class RlsServerProcess : IRlsServerProcess
    {
        private readonly Process process;

        internal RlsServerProcess(ProcessStartInfo startInfo)
        {
            process = new Process
            {
                EnableRaisingEvents = true,
                StartInfo = startInfo,
            };
            process.Exited += (_, eventArgs) => Exited?.Invoke(this, eventArgs);
            process.ErrorDataReceived += (_, eventArgs) =>
            {
                if (!string.IsNullOrEmpty(eventArgs.Data))
                {
                    ErrorReceived?.Invoke(eventArgs.Data);
                }
            };
        }

        public event EventHandler Exited;

        public event Action<string> ErrorReceived;

        public Stream StandardInput => process.StandardInput.BaseStream;

        public Stream StandardOutput => process.StandardOutput.BaseStream;

        public bool HasExited => process.HasExited;

        public int ExitCode => process.ExitCode;

        public bool Start() => process.Start();

        public void BeginErrorReadLine() => process.BeginErrorReadLine();

        public void Kill() => process.Kill();

        public void Dispose() => process.Dispose();
    }

    internal static class RlsServerConfiguration
    {
        internal static string ResolveServerPath(IRlsClientEnvironment environment)
        {
            if (!string.IsNullOrWhiteSpace(environment.ServerOverride))
            {
                return environment.ServerOverride;
            }

            return Path.Combine(
                environment.ExtensionDirectory,
                "Server",
                "rls_language_server.exe");
        }

        internal static ProcessStartInfo CreateStartInfo(string serverPath)
        {
            return new ProcessStartInfo
            {
                FileName = serverPath,
                WorkingDirectory = Path.GetDirectoryName(serverPath),
                RedirectStandardInput = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true,
            };
        }
    }
}