using System.ComponentModel.Composition;

using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Utilities;

namespace RandoLogicScript.VisualStudio
{
    internal static class RlsContentType
    {
        internal const string Name = "rls";

#pragma warning disable CS0649
        [Export]
        [Name(Name)]
        [BaseDefinition(CodeRemoteContentDefinition.CodeRemoteContentTypeName)]
        internal static ContentTypeDefinition ContentTypeDefinition;

        [Export]
        [FileExtension(".rls")]
        [ContentType(Name)]
        internal static FileExtensionToContentTypeDefinition FileExtensionDefinition;
#pragma warning restore CS0649
    }
}