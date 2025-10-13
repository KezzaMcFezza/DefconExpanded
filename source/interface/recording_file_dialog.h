#ifndef _included_recording_file_dialog_h
#define _included_recording_file_dialog_h

#include "lib/universal_include.h"
#include "interface/components/core.h"
#include "lib/tosser/llist.h"

//
// This file dialog is largely inspired from multiwinias map file dialog
// system, i duplicated the mod window and retrofitted it to load a file system
// instead of a static directory that we cannot escape

class RecordingSelectionWindow;
class ScrollBar;
class Bitmap;
class Image;

struct RecordingFileInfo
{
    char *filename;
    time_t modificationTime;
    
    RecordingFileInfo() : filename(NULL), modificationTime(0) {}
    ~RecordingFileInfo() { delete[] filename; }
};

class RecordingFileDialog : public InterfaceWindow
{
public:
    char    *m_currentPath;                     // current directory path
    char    *m_filter;                          // only show .dcrec files
    char    *m_selectedFile;                    // currently selected file
    
    RecordingSelectionWindow *m_parentWindow;   // callback target
    
    LList<RecordingFileInfo *> *m_files;        // files with date info
    LList<char *>   *m_directories;             // directories/folders only
    LList<int>      m_selected;                 // selected file i
    
    enum
    {
        SortTypeName,
        SortTypeDate
    };
    int m_sortType;
    bool m_sortInvert;
    
    enum
    {
        ShowFoldersFirst,
        ShowFilesFirst
    };
    int m_showOrder;
    
    ScrollBar       *m_scrollbar;               
    
    LList<char *>   m_navigationHistory;        // history of visited directories
    int             m_historyIndex;             // current position in history
    
    //
    // down the line it would be cool to show the territories from a selected
    // recording, it would use the same system as the lobby window i assume

    Bitmap          *m_bitmap;
    Image           *m_image;

    //
    // static member to store last folder in memory
    // this gets saved to preferences when game shuts down

    static char     *s_lastFolder;

public:
    RecordingFileDialog(RecordingSelectionWindow *parent);
    ~RecordingFileDialog();

    void SelectFile                    ( char *_filename );
    void SetCurrentDirectory           (const char *path);
    void NavigateInto                  (const char *dirName);
    void FileSelected                  (const char *filename);
    void NavigateBack();               // go up to parent directory
    void NavigateUpToParent();         // navigate to parent directory
    bool CanNavigateBack();            // check if parent directory exists
    void RefreshFileListing();
    void LoadMap();                    // load map.bmp
    void SortFileList();               // sort files by current criteria

    void Render                        ( bool _hasFocus );
    void Create();
    
    static void SaveLastFolderToPreferences();
    
private:
    char *GetFullPathForFile          (const char *filename);
    char *GetFullPathForDirectory     (const char *dirname);
    void CleanupLists();
};


#endif
