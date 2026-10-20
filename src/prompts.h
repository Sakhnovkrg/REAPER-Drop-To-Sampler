#pragma once

// Yes/No/Cancel questions for the "Always prompt" settings.
// Each returns the chosen mode, or -1 when cancelled.
int PromptSamplerChoice();
int PromptMultiFilesChoice();
int PromptMultiSamplersChoice();
