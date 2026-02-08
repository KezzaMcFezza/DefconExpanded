#ifndef INCLUDED_NATIVE_DIALOG_H
#define INCLUDED_NATIVE_DIALOG_H

/**
 * Native file/folder dialogs via SDL3.
 *
 * Uses the OS native picker:
 *   - Windows: Explorer (IFileDialog COM interface)
 *   - macOS: Finder (NSOpenPanel/NSSavePanel)
 *   - Linux: XDG Portals (KDE/GNOME file picker)
 *
 * All functions are asynchronous: they return immediately, and the result
 * is passed to the callback later.
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NativeDialogFilter {
    const char *name;
    const char *pattern;
} NativeDialogFilter;

typedef void (*NativeDialogCallback)(void *userdata, const char *const *paths, int filterIndex);

void NativeShowOpenFileDialog  (void *parentWindow, const char *defaultPath,
                                int allowMany, const NativeDialogFilter *filters, int nFilters,
                                NativeDialogCallback callback, void *userdata);
void NativeShowOpenFolderDialog(void *parentWindow, const char *defaultPath,
                                int allowMany, NativeDialogCallback callback, void *userdata);
void NativeShowSaveFileDialog  (void *parentWindow, const char *defaultPath,
                                const NativeDialogFilter *filters, int nFilters,
                                NativeDialogCallback callback, void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_NATIVE_DIALOG_H */
