#include "lib/universal_include.h"
#include "lib/render2d/renderer.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/filesys/file_system.h"
#include "lib/filesys/binary_stream_readers.h"
#include "lib/resource/bitmap.h"
#include "lib/resource/image.h"
#include "lib/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/string_utils.h"
#include "lib/language_table.h"
#include "lib/preferences.h"

#include "interface/components/message_dialog.h"
#include "interface/components/scrollbar.h"

#include "interface/recording_file_dialog.h"
#include "interface/recording_selection.h"
#include "interface/interface.h"

extern FileSystem *g_fileSystem;
extern Preferences *g_preferences;

//
// initialize static member

char *RecordingFileDialog::s_lastFolder = NULL;

#ifdef TARGET_MSVC
    #include <direct.h>
    #include <sys/stat.h>
    #define PATH_SEPARATOR "\\"
    #define getcwd _getcwd
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #define PATH_SEPARATOR "/"
#endif



class RecordingFileButton : public EclButton
{
public:
    int m_index;
    double m_lastClickTime; // double click detection

    RecordingFileButton() : m_lastClickTime(-1.0) {}

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        RecordingFileDialog *parent = (RecordingFileDialog *)m_parent;
        int actualIndex = parent->m_scrollbar->m_currentValue + m_index;

        int numDirectories = parent->m_directories ? parent->m_directories->Size() : 0;
        int numFiles = parent->m_files ? parent->m_files->Size() : 0;
        int totalItems = numDirectories + numFiles;

        if( actualIndex < totalItems )
        {
            g_renderer->EclipseRectFill( realX, realY, m_w, m_h, Colour(255,255,255,10), Colour(255,255,255,50), false );

            char *displayText = NULL;
            bool isDirectory = false;
            bool isSelected = false;
            
            if (parent->m_showOrder == RecordingFileDialog::ShowFoldersFirst)
            {

                //
                // folders first, then files

                if (actualIndex < numDirectories)
                {
                    displayText = parent->m_directories->GetData(actualIndex);
                    isDirectory = true;
                }
                else
                {
                    int fileIndex = actualIndex - numDirectories;
                    if (parent->m_files && parent->m_files->ValidIndex(fileIndex))
                    {
                        RecordingFileInfo *fileInfo = parent->m_files->GetData(fileIndex);
                        displayText = fileInfo->filename;
                        isSelected = (parent->m_selected.Size() > 0 && parent->m_selected[0] == fileIndex);
                    }
                }
            }
            else
            {
                //
                // files first, then folders
                
                if (actualIndex < numFiles)
                {
                    if (parent->m_files && parent->m_files->ValidIndex(actualIndex))
                    {
                        RecordingFileInfo *fileInfo = parent->m_files->GetData(actualIndex);
                        displayText = fileInfo->filename;
                        isSelected = (parent->m_selected.Size() > 0 && parent->m_selected[0] == actualIndex);
                    }
                }
                else
                {
                    int dirIndex = actualIndex - numFiles;
                    if (parent->m_directories && parent->m_directories->ValidIndex(dirIndex))
                    {
                        displayText = parent->m_directories->GetData(dirIndex);
                        isDirectory = true;
                    }
                }
            }
            
            if( isSelected )
            {
                g_renderer->EclipseRectFill( realX, realY, m_w, m_h, Colour(0,255,0,50) );
            }

            if( highlighted )
            {
                g_renderer->EclipseRectFill( realX, realY, m_w, m_h, Colour(255,255,255,50) );
                g_renderer->EclipseRect( realX, realY, m_w, m_h, Colour(255,255,255,200) );
            }
            
            if( isSelected )
            {
                g_renderer->EclipseRectFill( realX, realY, m_w, m_h, Colour(255,255,255,50) );
                g_renderer->EclipseRect( realX, realY, m_w, m_h, Colour(255,255,255,200) );
            }

            Colour textColour( 255, 255, 255, 200 );
            
            if( !isDirectory )
            {
                // green for .dcrec file text
                textColour.Set( 0, 255, 0, 200 );
            }

            if( displayText )
            {
                
                //
                // truncate names based on type, folders get 50 chars and dcrecs get 24 chars

                char displayName[64];
                
                if (isDirectory)
                {
                    TruncateText(displayName, displayText, 41, 0);
                }
                else
                {
                    TruncateText(displayName, displayText, 25, 5);
                }
                
                g_renderer->TextSimple( realX + 10, realY + 3, textColour, 12, displayName );
                
                //
                // show date for files

                if (!isDirectory && parent->m_files)
                {
                    int fileIndex = -1;
                    
                    if (parent->m_showOrder == RecordingFileDialog::ShowFoldersFirst)
                    {
                        if (actualIndex >= numDirectories)
                        {
                            fileIndex = actualIndex - numDirectories;
                        }
                    }
                    else
                    {
                        if (actualIndex < numFiles)
                        {
                            fileIndex = actualIndex;
                        }
                    }
                    
                    if (fileIndex >= 0 && parent->m_files->ValidIndex(fileIndex))
                    {
                        RecordingFileInfo *fileInfo = parent->m_files->GetData(fileIndex);
                        char dateStr[64];
                        struct tm *timeinfo = localtime(&fileInfo->modificationTime);
                        strftime(dateStr, sizeof(dateStr), "%m/%d/%y %H:%M", timeinfo);
                        
                        Colour dateColour(200, 200, 200, 150);
                        g_renderer->TextRightSimple( realX + m_w - 10, realY + 3, dateColour, 10, dateStr );
                    }
                }
            }
        }
    }

    void MouseUp()
    {
        RecordingFileDialog *parent = (RecordingFileDialog *)m_parent;

        if (!parent || !EclGetWindow(parent->m_name)) {
            return;  
        }
        
        int actualIndex = parent->m_scrollbar->m_currentValue + m_index;
        
        double timeNow = GetHighResTime();
        double timeDelta = timeNow - m_lastClickTime;
        
        int numDirectories = parent->m_directories ? parent->m_directories->Size() : 0;
        int numFiles = parent->m_files ? parent->m_files->Size() : 0;
        
        if (parent->m_showOrder == RecordingFileDialog::ShowFoldersFirst)
        {
            if (actualIndex < numDirectories)
            {
                //
                // delay on double click

                if (timeDelta < 0.3) 
                {
                    //
                    // lets go

                    char *dirName = parent->m_directories->GetData(actualIndex);
                    parent->NavigateInto(dirName);
                }

                //
                // just highlight if not double clicked
            }
            else
            {
                int fileIndex = actualIndex - numDirectories;
                if (parent->m_files && parent->m_files->ValidIndex(fileIndex))
                {
                    RecordingFileInfo *fileInfo = parent->m_files->GetData(fileIndex);
                    if (timeDelta < 0.3)
                    {
                        parent->FileSelected(fileInfo->filename);
                    }
                    else
                    {

                        //
                        // single click just selects/highlights the file
                        
                        parent->SelectFile(fileInfo->filename);
                    }
                }
            }
        }
        else
        {
            if (actualIndex < numFiles)
            {
                if (parent->m_files && parent->m_files->ValidIndex(actualIndex))
                {
                    RecordingFileInfo *fileInfo = parent->m_files->GetData(actualIndex);
                    if (timeDelta < 0.3)
                    {
                        parent->FileSelected(fileInfo->filename);
                    }
                    else
                    {

                        //
                        // single click just selects/highlights the file
                        
                        parent->SelectFile(fileInfo->filename);
                    }
                }
            }
            else
            {
                int dirIndex = actualIndex - numFiles;
                if (parent->m_directories && parent->m_directories->ValidIndex(dirIndex))
                {
                    //
                    // delay on double click

                    if (timeDelta < 0.3) 
                    {
                        //
                        // lets go

                        char *dirName = parent->m_directories->GetData(dirIndex);
                        parent->NavigateInto(dirName);
                    }

                    //
                    // just highlight if not double clicked
                }
            }
        }
        
        m_lastClickTime = timeNow;
    }
};


class RecordingSortButton : public InterfaceButton
{
public:
    void MouseUp()
    {
        RecordingFileDialog *parent = (RecordingFileDialog *)m_parent;
        
        // Toggle between Name and Date
        if (parent->m_sortType == RecordingFileDialog::SortTypeName)
        {
            parent->m_sortType = RecordingFileDialog::SortTypeDate;
        }
        else
        {
            parent->m_sortType = RecordingFileDialog::SortTypeName;
        }
        
        parent->SortFileList();
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {      
        RecordingFileDialog *parent = (RecordingFileDialog *)m_parent;

        g_renderer->EclipseRectFill( realX, realY, m_w, m_h, Colour(200,200,255,20) );

        if( highlighted ) 
        {
            g_renderer->EclipseRectFill( realX, realY, m_w, m_h, Colour(200,200,255,50) );
        }

        //
        // Render the caption based on current sort type

        const char* caption = (parent->m_sortType == RecordingFileDialog::SortTypeName) ? "Name" : "Date";
        g_renderer->Text( realX + 10, realY+3, Colour(200,200,255,255), 12, caption );
    }
};

class RecordingShowOrderButton : public InterfaceButton
{
public:
    void MouseUp()
    {
        RecordingFileDialog *parent = (RecordingFileDialog *)m_parent;
        
        // Toggle between Folders and Dcrecs
        if (parent->m_showOrder == RecordingFileDialog::ShowFoldersFirst)
        {
            parent->m_showOrder = RecordingFileDialog::ShowFilesFirst;
        }
        else
        {
            parent->m_showOrder = RecordingFileDialog::ShowFoldersFirst;
        }
        
        parent->RefreshFileListing();
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {      
        RecordingFileDialog *parent = (RecordingFileDialog *)m_parent;

        g_renderer->EclipseRectFill( realX, realY, m_w, m_h, Colour(200,200,255,20) );

        if( highlighted ) 
        {
            g_renderer->EclipseRectFill( realX, realY, m_w, m_h, Colour(200,200,255,50) );
        }

        //
        // Render the caption based on current show order

        const char* caption = (parent->m_showOrder == RecordingFileDialog::ShowFoldersFirst) ? "Folders" : "Dcrecs";
        g_renderer->Text( realX + 10, realY+3, Colour(200,200,255,255), 12, caption );
    }
};

class RecordingBackButton : public InterfaceButton
{
public:
    bool IsVisible()
    {
        RecordingFileDialog *parent = (RecordingFileDialog *)m_parent;
        return parent->CanNavigateBack();
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( IsVisible() )
        {
            InterfaceButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        if( IsVisible() )
        {
            RecordingFileDialog *parent = (RecordingFileDialog *)m_parent;
            parent->NavigateBack();
        }
    }
};

class RecordingFileOKButton : public InterfaceButton
{
public:
    void MouseUp()
    {
        RecordingFileDialog *parent = (RecordingFileDialog *)m_parent;
        
        if (parent->m_selectedFile && strlen(parent->m_selectedFile) > 0)
        {
            parent->FileSelected(parent->m_selectedFile);
        }
    }
};

class RecordingFileCancelButton : public InterfaceButton
{
public:
    void MouseUp()
    {
        RecordingFileDialog *parent = (RecordingFileDialog *)m_parent;
        EclRemoveWindow(parent->m_name);
    }
};




RecordingFileDialog::RecordingFileDialog(RecordingSelectionWindow *parent)
:   InterfaceWindow( "RecordingFileBrowser", "Select Recording File", false ),
    m_currentPath(NULL),
    m_filter(NULL),
    m_selectedFile(NULL),
    m_parentWindow(parent),
    m_files(NULL),
    m_directories(NULL),
    m_scrollbar(NULL),
    m_historyIndex(-1),
    m_bitmap(NULL),
    m_image(NULL),
    m_sortType(SortTypeDate),
    m_sortInvert(true),
    m_showOrder(RecordingFileDialog::ShowFoldersFirst)
{
        SetSize( 560, 400 );
        Centralise();

        //
        // send the user to their download folder if possible
        // it works on windows and linux, yet to be tested on macs

        char *downloadsPath = NULL;
    
        //
        // first try to load the last used folder from memory (static member)
        // if not in memory, try preferences
        // if not in preferences, fall back to downloads folder

        if (s_lastFolder && IsDirectory(s_lastFolder))
        {
            m_currentPath = newStr(s_lastFolder);
            AppDebugOut("RecordingFileDialog: Loaded last folder from memory '%s'\n", m_currentPath);
        }
        else
        {
        const char *lastFolder = g_preferences->GetString("RecordingLastFolder", NULL);
        if (lastFolder && IsDirectory(lastFolder))
        {
            m_currentPath = newStr(lastFolder);
            AppDebugOut("RecordingFileDialog: Loaded last folder from preferences '%s'\n", m_currentPath);
        }
        else
        {
        //
        // fallback to downloads folder if no saved preference or folder doesn't exist

#ifdef TARGET_MSVC
        //
        // use %USERPROFILE%\Downloads

        const char *userProfile = getenv("USERPROFILE");
        if (userProfile)
        {
            downloadsPath = ConcatPaths(userProfile, "Downloads", NULL);
        }
        else
        {
            //
            // if USERPROFILE is not available or doesnt work

            downloadsPath = newStr("C:\\Users\\Public\\Downloads");
        }
#elif defined(TARGET_OS_MACOSX)

        //
        // use ~/Downloads

        const char *home = getenv("HOME");
        if (home)
        {
            downloadsPath = ConcatPaths(home, "Downloads", NULL);
        }
        else
        {
            downloadsPath = newStr("/Users/Shared/Downloads");
        }
#elif defined(TARGET_OS_LINUX)

        //
        // same as macos

        const char *home = getenv("HOME");
        if (home)
        {
            downloadsPath = ConcatPaths(home, "Downloads", NULL);
        }
        else
        {
            downloadsPath = newStr("/tmp");
        }
#endif
        //
        // if downloads folder doesnt exsit, fallback to root :(

        if (downloadsPath && IsDirectory(downloadsPath))
        {
            m_currentPath = downloadsPath;
            AppDebugOut("RecordingFileDialog: Found Downloads folder '%s'\n", m_currentPath);
        }
        else
        {
            // fallback to root directory
            delete[] downloadsPath;
#ifdef TARGET_MSVC
            m_currentPath = newStr("C:\\");
#elif defined(TARGET_OS_MACOSX)
            m_currentPath = newStr("/");
#elif defined(TARGET_OS_LINUX)
            m_currentPath = newStr("/");
#endif
            AppDebugOut("RecordingFileDialog: Downloads folder not found, using root directory '%s'\n", m_currentPath);
        }
    }
}
    //
    // dcrecs only

    m_filter = newStr("*.dcrec");
    
    //
    // add initial directory to navigation history

    m_navigationHistory.PutData(newStr(m_currentPath));
    m_historyIndex = 0;
    
    AppDebugOut("RecordingFileDialog: Started in directory '%s'\n", m_currentPath);
}

RecordingFileDialog::~RecordingFileDialog()
{
    CleanupLists();
    
    delete[] m_currentPath;
    delete[] m_filter;
    delete[] m_selectedFile;
    
    m_navigationHistory.EmptyAndDeleteArray();

}


void RecordingFileDialog::Create()
{
    InterfaceWindow::Create();

    int listW = 320;

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "box", 10, 25, listW, m_h - 60, " ", " ", false, false );
    RegisterButton( box );

    //
    // one button for each possible file/folder

    int itemH = 20;
    int itemG = 2;
    int maxItems = (m_h - 100) / (itemH + itemG);
    int y = 35;
    int itemX = 20;
    int itemW = listW - 40;

    for( int i = 0; i < maxItems; ++i )
    {
        char name[256];
        sprintf( name, "file %d", i );

        RecordingFileButton *button = new RecordingFileButton();
        button->SetProperties( name, itemX, y+=itemH+itemG, itemW, itemH, name, " ", false, false );
        button->m_index = i;
        RegisterButton( button );
    }

    //
    // sort button

    RecordingSortButton *sortButton = new RecordingSortButton();
    sortButton->SetProperties( "Sort", 20, 30, 80, 20, "Name", " ", false, false );
    RegisterButton( sortButton );

    //
    // show order button

    RecordingShowOrderButton *showOrderButton = new RecordingShowOrderButton();
    showOrderButton->SetProperties( "ShowOrder", 105, 30, 80, 20, "Folders", " ", false, false );
    RegisterButton( showOrderButton );

    //
    // navigation and control buttons 

    RecordingBackButton *backBtn = new RecordingBackButton();
    backBtn->SetProperties( "Back", 10, m_h - 30, 100, 20, "dialog_back", "Go back in history", true, false );
    RegisterButton( backBtn );

    RecordingFileOKButton *okBtn = new RecordingFileOKButton();
    okBtn->SetProperties( "FileOK", m_w - 220, m_h - 30, 100, 20, "dialog_load", "Select this file", true, false );
    RegisterButton( okBtn );

    RecordingFileCancelButton *cancelBtn = new RecordingFileCancelButton();
    cancelBtn->SetProperties( "FileCancel", m_w - 110, m_h - 30, 100, 20, "dialog_close", "Close without selecting", true, false );
    RegisterButton( cancelBtn );

    //
    // scrollbar

    m_scrollbar = new ScrollBar( this );
    m_scrollbar->Create( "scroll", 10 + listW - 27, 57, 18, m_h - 110, 0, maxItems );        
    
    //
    // refresh the file listing and load directory image

    RefreshFileListing();
    LoadMap();
}

//
// select file

void RecordingFileDialog::SelectFile( char *_filename )
{
    m_selected.Empty();
    
    // update selected file
    delete[] m_selectedFile;
    m_selectedFile = NULL;
    
    if (_filename && strlen(_filename) > 0)
    {
        m_selectedFile = newStr(_filename);
        
        //
        // find the file index in our list for selection highlighting

        if (m_files)
        {
            for (int i = 0; i < m_files->Size(); ++i)
            {
                RecordingFileInfo *fileInfo = m_files->GetData(i);
                if (strcmp(fileInfo->filename, _filename) == 0)
                {
                    m_selected.PutData(i);
                    break;
                }
            }
        }
    }
}

void RecordingFileDialog::SetCurrentDirectory(const char *path)
{
    if (path && IsDirectory(path))
    {
        delete[] m_currentPath;
        m_currentPath = newStr(path);
        
        //
        // store in memory for next time

        delete[] s_lastFolder;
        s_lastFolder = newStr(path);
        
        RefreshFileListing();
    }
}

void RecordingFileDialog::NavigateBack()
{
    NavigateUpToParent();
}

void RecordingFileDialog::NavigateUpToParent()
{
    if (!m_currentPath) return;
    
    char *lastSeparator = strrchr(m_currentPath, PATH_SEPARATOR[0]);
    
    char *parentPath = NULL;
    
    if (lastSeparator && lastSeparator != m_currentPath)
    {
        //
        // create parent path

        size_t parentLen = lastSeparator - m_currentPath;
        parentPath = new char[parentLen + 1];
        strncpy(parentPath, m_currentPath, parentLen);
        parentPath[parentLen] = '\0';
    }
#ifdef TARGET_MSVC
    //
    // on Windows, handle drive root specially

    else if (strlen(m_currentPath) > 3 && m_currentPath[1] == ':')
    {
        parentPath = new char[4];
        strncpy(parentPath, m_currentPath, 3);
        parentPath[3] = '\0';
    }
#endif
    
    if (parentPath && IsDirectory(parentPath))
    {
        
        //
        // add parent path to history

        m_navigationHistory.PutData(newStr(parentPath));
        m_historyIndex++;
        
        //
        // update current path

        delete[] m_currentPath;
        m_currentPath = newStr(parentPath);
        
        //
        // store in memory for next time

        delete[] s_lastFolder;
        s_lastFolder = newStr(parentPath);
        
        RefreshFileListing();
        
    }
    
    if (parentPath)
        delete[] parentPath;
}

bool RecordingFileDialog::CanNavigateBack()
{
    //
    // can we go to parent directory?

    if (!m_currentPath) 
        return false;
    
    //
    // find the last path separator

    char *lastSeparator = strrchr(m_currentPath, PATH_SEPARATOR[0]);
    
    //
    // moment of truth

    if (lastSeparator && lastSeparator != m_currentPath)
        return true; // yes we can
        
#ifdef TARGET_MSVC
    // we can go up if were not at drive root
    if (strlen(m_currentPath) > 3 && m_currentPath[1] == ':')
        return true;
#endif
    
    return false; // at root directory, cant go back anymore
}

//
// enter a directory

void RecordingFileDialog::NavigateInto(const char *dirName)
{
    char *fullPath = GetFullPathForDirectory(dirName);
    if (fullPath && IsDirectory(fullPath))
    {
        
        m_navigationHistory.PutData(newStr(fullPath));
        m_historyIndex++;
        
        delete[] m_currentPath;
        m_currentPath = newStr(fullPath);
        
        //
        // store in memory for next time

        delete[] s_lastFolder;
        s_lastFolder = newStr(fullPath);
        
        RefreshFileListing();
        
        delete[] fullPath;
        
    }
    else if (fullPath)
    {
        delete[] fullPath;
    }
}

//
// refresh whenever we do anything at all

void RecordingFileDialog::RefreshFileListing()
{
    CleanupLists();
    
    //
    // ensure path ends with separator for proper file system enumeration

    char searchPath[512];
    strncpy(searchPath, m_currentPath, sizeof(searchPath) - 2);
    searchPath[sizeof(searchPath) - 2] = '\0';
    
    //
    // add trailing separator if not present

    size_t pathLen = strlen(searchPath);
    if (pathLen > 0 && searchPath[pathLen - 1] != PATH_SEPARATOR[0])
    {
        strcat(searchPath, PATH_SEPARATOR);
    }
    
    //
    // get list of directories 

    m_directories = ListSubDirectoryNames(m_currentPath);
    
    //
    // get list of files with modification times

    m_files = new LList<RecordingFileInfo *>();
    
    LList<char *> *tempFiles = NULL;
    if (g_fileSystem)
    {
        tempFiles = g_fileSystem->ListArchive(searchPath, m_filter, false);
    }
    else
    {
        tempFiles = ListDirectory(m_currentPath, m_filter, false);
    }
    
    if (tempFiles)
    {
        for (int i = 0; i < tempFiles->Size(); ++i)
        {
            char *filename = tempFiles->GetData(i);
            char *fullPath = GetFullPathForFile(filename);
            
            RecordingFileInfo *fileInfo = new RecordingFileInfo();
            fileInfo->filename = newStr(filename);
            
            // Get file modification time
            struct stat fileStat;
            if (stat(fullPath, &fileStat) == 0)
            {
                fileInfo->modificationTime = fileStat.st_mtime;
            }
            else
            {
                fileInfo->modificationTime = 0;
            }
            
            m_files->PutData(fileInfo);
            delete[] fullPath;
        }
        
        tempFiles->EmptyAndDeleteArray();
        delete tempFiles;
    }
    
    m_selected.Empty();
    SortFileList();
    
    //
    // update scroll bar

    int totalItems = (m_directories ? m_directories->Size() : 0) + 
                     (m_files ? m_files->Size() : 0);
    m_scrollbar->SetNumRows(totalItems);
    m_scrollbar->SetCurrentValue(0);
    
}

void RecordingFileDialog::FileSelected(const char *filename)
{
    if (m_parentWindow && filename)
    {
        char *fullPath = GetFullPathForFile(filename);
        if (fullPath)
        {
            //
            // update parent window with selected file

            strncpy(m_parentWindow->m_recordingFilename, fullPath, 
                    sizeof(m_parentWindow->m_recordingFilename) - 1);
            m_parentWindow->m_recordingFilename[sizeof(m_parentWindow->m_recordingFilename) - 1] = '\0';
            
            //
            // update parent window title

            m_parentWindow->SetTitle("Recording Playback (File Selected)");
            
            //
            // store the current folder in memory for next time
            // preferences will be saved when game shuts down

            if (m_currentPath)
            {
                delete[] s_lastFolder;
                s_lastFolder = newStr(m_currentPath);
            }
            
            delete[] fullPath;
        }
    }
    
    EclRemoveWindow(m_name);
}

void RecordingFileDialog::CleanupLists()
{
    if (m_files)
    {
        m_files->EmptyAndDelete();
        delete m_files;
        m_files = NULL;
    }
    
    if (m_directories)
    {
        m_directories->EmptyAndDeleteArray();
        delete m_directories;
        m_directories = NULL;
    }
}

char *RecordingFileDialog::GetFullPathForFile(const char *filename)
{
    if (!filename || !m_currentPath) return NULL;
    
    size_t pathLen = strlen(m_currentPath);
    size_t fileLen = strlen(filename);
    size_t totalLen = pathLen + 1 + fileLen + 1; // path + separator + file + null
    
    char *fullPath = new char[totalLen];
    snprintf(fullPath, totalLen, "%s%s%s", m_currentPath, PATH_SEPARATOR, filename);
    
    return fullPath;
}

char *RecordingFileDialog::GetFullPathForDirectory(const char *dirname)
{
    return GetFullPathForFile(dirname); 
}

void RecordingFileDialog::SortFileList()
{
    if (!m_files || m_files->Size() <= 1) return;
    
    //
    // create temporary array for sorting

    int numFiles = m_files->Size();
    RecordingFileInfo **tempArray = new RecordingFileInfo*[numFiles];
    
    //
    // copy pointers to temporary array

    for (int i = 0; i < numFiles; ++i)
    {
        tempArray[i] = m_files->GetData(i);
    }
    
    //
    // execute bubble sort on the array

    for (int i = 0; i < numFiles - 1; ++i)
    {
        for (int j = 0; j < numFiles - i - 1; ++j)
        {
            RecordingFileInfo *file1 = tempArray[j];
            RecordingFileInfo *file2 = tempArray[j + 1];
            
            bool shouldSwap = false;
            
            switch (m_sortType)
            {
                case SortTypeName:
                    if (m_sortInvert)
                        shouldSwap = (strcmp(file1->filename, file2->filename) < 0);
                    else
                        shouldSwap = (strcmp(file1->filename, file2->filename) > 0);
                    break;
                    
                case SortTypeDate:
                    if (m_sortInvert)
                        shouldSwap = (file1->modificationTime < file2->modificationTime);
                    else
                        shouldSwap = (file1->modificationTime > file2->modificationTime);
                    break;
            }
            
            if (shouldSwap)
            {
                RecordingFileInfo *temp = tempArray[j];
                tempArray[j] = tempArray[j + 1];
                tempArray[j + 1] = temp;
            }
        }
    }
    
    //
    // clear the list and rebuild it with sorted order

    m_files->Empty();
    for (int i = 0; i < numFiles; ++i)
    {
        m_files->PutDataAtEnd(tempArray[i]);
    }
    
    delete[] tempArray;
}

void RecordingFileDialog::LoadMap()
{
    //
    // clean up image and bitmap but DONT delete them

    if (m_image)
    {
        m_image = NULL;
    }

    if (m_bitmap)
    {
        m_bitmap = NULL;
    }

    //
    // load map.bmp directly from resource system regardless of file selected

    m_image = g_resource->GetImage("graphics/map.bmp");

}

//
// static method to save last folder to preferences
// called when game shuts down

void RecordingFileDialog::SaveLastFolderToPreferences()
{
    if (s_lastFolder && strlen(s_lastFolder) > 0)
    {
        g_preferences->SetString("RecordingLastFolder", s_lastFolder);
        g_preferences->Save();
        AppDebugOut("RecordingFileDialog: Saved last folder to preferences on shutdown '%s'\n", s_lastFolder);
    }
}

//
// render it

void RecordingFileDialog::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    int xPos = m_x + m_w - 220;
    int w = 210;

    if( m_image )
    {
        g_renderer->EclipseSprite( m_image, xPos, m_y + 30, w, 118, White );
    }
    else
    {
        g_renderer->TextCentreSimple( xPos+w/2, m_y + 60, White, 14, LANGUAGEPHRASE("dialog_mod_screenshot_not_found") );
    }

    g_renderer->EclipseRect( xPos, m_y + 30, w, 118, Colour(200,200,255,200) );

    //
    // directory and file information 

    int yPos = m_y + 160;
    int fontSize = 11;
    int gap = 15;
    Colour nameColour(200,200,255,200);

    //
    // Current folder info, show folder name and not path

    g_renderer->TextSimple( xPos, yPos, nameColour, fontSize, "Current Folder:" );
    if (m_currentPath)
    {
        // get folder name
        const char* folderName = strrchr(m_currentPath, PATH_SEPARATOR[0]);
        if (folderName)
        {
            folderName++;
        }
        else
        {
            folderName = m_currentPath;
        }
        
        //
        // Handle empty folder name

        if (strlen(folderName) == 0)
        {
#ifdef TARGET_MSVC
            //
            // show drive letter on Windows

            folderName = m_currentPath;
#else
            //
            // on unix, show root

            folderName = "/"; // Show root on Unix
#endif
        }
        
        char displayFolderName[64];
        TruncateText(displayFolderName, folderName, 40, 0);
        
        g_renderer->TextRightSimple( xPos + w, yPos, White, fontSize, displayFolderName );
    }
    yPos += gap;

    //
    // file counts

    int numDirs = m_directories ? m_directories->Size() : 0;
    int numFiles = m_files ? m_files->Size() : 0;
    
    g_renderer->TextSimple( xPos, yPos, nameColour, fontSize, "Folders:" );
    char dirCountStr[32];
    sprintf(dirCountStr, "%d", numDirs);
    g_renderer->TextRightSimple( xPos + w, yPos, White, fontSize, dirCountStr );
    yPos += gap;
    
    g_renderer->TextSimple( xPos, yPos, nameColour, fontSize, "Recordings:" );
    char fileCountStr[32];
    sprintf(fileCountStr, "%d", numFiles);
    g_renderer->TextRightSimple( xPos + w, yPos, Colour(0,255,0,200), fontSize, fileCountStr );
    yPos += gap;

    //
    // selected file info

    if (m_selectedFile)
    {
        g_renderer->TextSimple( xPos, yPos, nameColour, fontSize, "Selected:" );
        

        char displaySelectedName[32];
        TruncateText(displaySelectedName, m_selectedFile, 25, 5);
        
        g_renderer->TextRightSimple( xPos + w, yPos, Colour(0,255,0,200), fontSize - 1, displaySelectedName );
    }
    else
    {
        g_renderer->TextSimple( xPos, yPos, Colour(150,150,150,200), fontSize, "No file selected" );
    }
}
