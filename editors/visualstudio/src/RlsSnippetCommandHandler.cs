using System;
using System.ComponentModel.Composition;
using System.Runtime.InteropServices;
using System.Text;

using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Editor;
using Microsoft.VisualStudio.OLE.Interop;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Text.Operations;
using Microsoft.VisualStudio.TextManager.Interop;
using Microsoft.VisualStudio.Utilities;

using MSXML;

namespace RandoLogicScript.VisualStudio
{
    [Export(typeof(IVsTextViewCreationListener))]
    [ContentType(RlsContentType.Name)]
    [TextViewRole(PredefinedTextViewRoles.Editable)]
    internal sealed class RlsSnippetCommandHandlerProvider : IVsTextViewCreationListener
    {
        [Import]
        internal IVsEditorAdaptersFactoryService EditorAdaptersFactoryService { get; set; }

        [Import]
        internal ITextStructureNavigatorSelectorService NavigatorService { get; set; }

        [Import]
        internal SVsServiceProvider ServiceProvider { get; set; }

        public void VsTextViewCreated(IVsTextView textViewAdapter)
        {
            var textView = EditorAdaptersFactoryService.GetWpfTextView(textViewAdapter);
            if (textView == null)
            {
                return;
            }

            textView.Properties.GetOrCreateSingletonProperty(() =>
                new RlsSnippetCommandHandler(
                    textViewAdapter,
                    textView,
                    NavigatorService,
                    ServiceProvider));
        }
    }

    internal sealed class RlsSnippetCommandHandler : IOleCommandTarget, IVsExpansionClient
    {
        internal const string LanguageServiceGuidString = "4C159F73-D995-4A40-ACD4-A04BB3DE2118";

        private static readonly Guid LanguageServiceGuid = new Guid(LanguageServiceGuidString);

        private readonly IVsTextView textViewAdapter;
        private readonly ITextView textView;
        private readonly ITextStructureNavigatorSelectorService navigatorService;
        private readonly IVsExpansionManager expansionManager;
        private IOleCommandTarget nextCommandHandler;
        private IVsExpansionSession expansionSession;
        private string insertionIndentation;

        internal RlsSnippetCommandHandler(
            IVsTextView textViewAdapter,
            ITextView textView,
            ITextStructureNavigatorSelectorService navigatorService,
            System.IServiceProvider serviceProvider)
        {
            this.textViewAdapter = textViewAdapter;
            this.textView = textView;
            this.navigatorService = navigatorService;

            var textManager = serviceProvider.GetService(typeof(SVsTextManager)) as IVsTextManager2;
            textManager?.GetExpansionManager(out expansionManager);
            textViewAdapter.AddCommandFilter(this, out nextCommandHandler);
        }

        public int QueryStatus(
            ref Guid commandGroup, uint commandCount, OLECMD[] commands, IntPtr commandText)
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            if (commandGroup == VSConstants.VSStd2K && commandCount > 0
                && commands[0].cmdID == (uint)VSConstants.VSStd2KCmdID.INSERTSNIPPET)
            {
                commands[0].cmdf = (uint)(OLECMDF.OLECMDF_ENABLED | OLECMDF.OLECMDF_SUPPORTED);
                return VSConstants.S_OK;
            }

            return nextCommandHandler.QueryStatus(
                ref commandGroup, commandCount, commands, commandText);
        }

        public int Exec(
            ref Guid commandGroup, uint commandId, uint commandOptions,
            IntPtr input, IntPtr output)
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            if (commandGroup == VSConstants.VSStd2K)
            {
                if (commandId == (uint)VSConstants.VSStd2KCmdID.INSERTSNIPPET)
                {
                    return InvokeInsertionUI();
                }

                if (expansionSession != null)
                {
                    if (commandId == (uint)VSConstants.VSStd2KCmdID.TAB)
                    {
                        expansionSession.GoToNextExpansionField(0);
                        return VSConstants.S_OK;
                    }
                    if (commandId == (uint)VSConstants.VSStd2KCmdID.BACKTAB)
                    {
                        expansionSession.GoToPreviousExpansionField();
                        return VSConstants.S_OK;
                    }
                    if (commandId == (uint)VSConstants.VSStd2KCmdID.RETURN
                        || commandId == (uint)VSConstants.VSStd2KCmdID.CANCEL)
                    {
                        expansionSession.EndCurrentExpansion(0);
                        expansionSession = null;
                        return VSConstants.S_OK;
                    }
                }

                if (commandId == (uint)VSConstants.VSStd2KCmdID.TAB
                    && TryGetShortcutBeforeCaret(out var shortcut)
                    && InsertExpansion(shortcut, null, null))
                {
                    return VSConstants.S_OK;
                }
            }

            return nextCommandHandler.Exec(
                ref commandGroup, commandId, commandOptions, input, output);
        }

        private int InvokeInsertionUI()
        {
            if (expansionManager == null)
            {
                return VSConstants.E_FAIL;
            }

            return expansionManager.InvokeInsertionUI(
                textViewAdapter,
                this,
                LanguageServiceGuid,
                null,
                0,
                0,
                null,
                0,
                0,
                "Rando Logic Script snippets",
                string.Empty);
        }

        private bool TryGetShortcutBeforeCaret(out string shortcut)
        {
            shortcut = null;
            var caret = textView.Caret.Position.BufferPosition;
            if (caret.Position == 0)
            {
                return false;
            }

            var navigator = navigatorService.GetTextStructureNavigator(textView.TextBuffer);
            var extent = navigator.GetExtentOfWord(caret - 1);
            if (!extent.IsSignificant)
            {
                return false;
            }

            shortcut = extent.Span.GetText();
            return !string.IsNullOrWhiteSpace(shortcut);
        }

        private bool InsertExpansion(string shortcut, string title, string path)
        {
            if (expansionManager == null)
            {
                return false;
            }

            textViewAdapter.GetCaretPos(out var line, out var column);
            insertionIndentation = GetLineIndentation(line, column);
            try
            {
                var insertionSpan = new TextSpan
                {
                    iStartLine = line,
                    iEndLine = line,
                    iStartIndex = column,
                    iEndIndex = column,
                };

                if (shortcut != null)
                {
                    insertionSpan.iStartIndex -= shortcut.Length;
                    var spans = new[] { insertionSpan };
                    if (ErrorHandler.Failed(expansionManager.GetExpansionByShortcut(
                            this,
                            LanguageServiceGuid,
                            shortcut,
                            textViewAdapter,
                            spans,
                            0,
                            out path,
                            out title)))
                    {
                        return false;
                    }
                }

                if (string.IsNullOrEmpty(title) || string.IsNullOrEmpty(path))
                {
                    return false;
                }

                textViewAdapter.GetBuffer(out var textLines);
                if (!(textLines is IVsExpansion expansion))
                {
                    return false;
                }

                return ErrorHandler.Succeeded(expansion.InsertNamedExpansion(
                    title,
                    path,
                    insertionSpan,
                    this,
                    LanguageServiceGuid,
                    0,
                    out expansionSession));
            }
            finally
            {
                insertionIndentation = null;
            }
        }

        private string GetLineIndentation(int lineNumber, int column)
        {
            var snapshot = textView.TextSnapshot;
            if (lineNumber < 0 || lineNumber >= snapshot.LineCount)
            {
                return string.Empty;
            }

            string line = snapshot.GetLineFromLineNumber(lineNumber).GetText();
            int prefixLength = Math.Min(column, line.Length);
            for (int index = 0; index < prefixLength; ++index)
            {
                if (line[index] != ' ' && line[index] != '\t')
                {
                    return string.Empty;
                }
            }
            return line.Substring(0, prefixLength);
        }

        internal static string ApplyBaseIndentation(string text, string indentation)
        {
            if (string.IsNullOrEmpty(text) || string.IsNullOrEmpty(indentation))
            {
                return text;
            }

            var result = new StringBuilder(text.Length + indentation.Length * 2);
            for (int index = 0; index < text.Length; ++index)
            {
                char character = text[index];
                result.Append(character);
                if (character == '\n' && index + 1 < text.Length)
                {
                    result.Append(indentation);
                }
            }
            return result.ToString();
        }

        public int EndExpansion()
        {
            expansionSession = null;
            return VSConstants.S_OK;
        }

        public int FormatSpan(IVsTextLines buffer, TextSpan[] spans)
        {
            if (string.IsNullOrEmpty(insertionIndentation) || spans == null)
            {
                return VSConstants.S_OK;
            }

            var snapshot = textView.TextSnapshot;
            using (var edit = textView.TextBuffer.CreateEdit())
            {
                foreach (var span in spans)
                {
                    if (span.iStartLine < 0 || span.iStartLine >= snapshot.LineCount
                        || span.iEndLine < span.iStartLine || span.iEndLine >= snapshot.LineCount)
                    {
                        continue;
                    }

                    var startLine = snapshot.GetLineFromLineNumber(span.iStartLine);
                    var endLine = snapshot.GetLineFromLineNumber(span.iEndLine);
                    int start = startLine.Start.Position + span.iStartIndex;
                    int end = endLine.Start.Position + span.iEndIndex;
                    if (start < 0 || end < start || end > snapshot.Length)
                    {
                        continue;
                    }

                    string original = snapshot.GetText(start, end - start);
                    string formatted = ApplyBaseIndentation(original, insertionIndentation);
                    if (!string.Equals(original, formatted, StringComparison.Ordinal))
                    {
                        edit.Replace(start, end - start, formatted);
                    }
                }
                edit.Apply();
            }
            return VSConstants.S_OK;
        }

        public int GetExpansionFunction(
            IXMLDOMNode functionNode, string fieldName, out IVsExpansionFunction function)
        {
            function = null;
            return VSConstants.S_OK;
        }

        public int IsValidKind(
            IVsTextLines buffer, TextSpan[] span, string kind, out int isValid)
        {
            isValid = 1;
            return VSConstants.S_OK;
        }

        public int IsValidType(
            IVsTextLines buffer, TextSpan[] span, string[] types,
            int typeCount, out int isValid)
        {
            isValid = 1;
            return VSConstants.S_OK;
        }

        public int OnAfterInsertion(IVsExpansionSession session) => VSConstants.S_OK;

        public int OnBeforeInsertion(IVsExpansionSession session) => VSConstants.S_OK;

        public int OnItemChosen(string title, string path)
        {
            return InsertExpansion(null, title, path) ? VSConstants.S_OK : VSConstants.E_FAIL;
        }

        public int PositionCaretForEditing(IVsTextLines buffer, TextSpan[] span) => VSConstants.S_OK;
    }
}