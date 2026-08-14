import { runLanguageClientTest } from './languageClient.test';

export async function run(): Promise<void> {
  await runLanguageClientTest();
}