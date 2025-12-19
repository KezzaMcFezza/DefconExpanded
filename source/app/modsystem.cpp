#include "lib/universal_include.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/filesys/file_system.h"
#include "lib/filesys/text_stream_readers.h"
#include "lib/gucci/window_manager.h"
#include "lib/resource/resource.h"
#include "lib/resource/sprite_atlas.h"
#include "lib/metaserver/authentication.h"
#include "lib/render/styletable.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"
#include "lib/sound/soundsystem.h"
#include "lib/sound/sound_sample_bank.h"
#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/preferences.h"
#include "lib/language_table.h"

#include "renderer/world_renderer.h"
#include "renderer/map_renderer.h"
#include "renderer/globe_renderer.h"

#include "interface/interface.h"
#include "lib/eclipse/components/message_dialog.h"

#include "globals.h"
#include "app.h"
#include "modsystem.h"

#include "world/earthdata.h"


ModSystem *g_modSystem = NULL;

ModSystem::ModSystem()
{
	m_modsDir = newStr("mods");
#ifdef TARGET_OS_MACOSX
    if ( getenv("HOME") )
	{
		delete[] m_modsDir;
		m_modsDir = ConcatPaths( getenv("HOME"), "Library/Application Support/DEFCON/mods", NULL );
	}
#endif
}


ModSystem::~ModSystem()
{
	delete[] m_modsDir;

    m_mods.EmptyAndDelete();
    m_criticalFiles.EmptyAndDeleteArray();
    ClearModGraphicsCache();
    m_geographyAffectingMods.EmptyAndDeleteArray();
    m_styleAffectingMods.EmptyAndDeleteArray();
}


void ModSystem::Initialise()
{
    //
    // Load the critical file list

    TextFileReader *reader = (TextFileReader *)g_fileSystem->GetTextReader( "data/critical_files.txt" );
    if( reader )
    {
        while( reader->ReadLine() )
        {
            char *thisFile = reader->GetNextToken();
            m_criticalFiles.PutData( newStr( thisFile ) );
        }

        delete reader;
    }

	// Ensure mods directory exists, just as a convenience to the
	// user. Wouldn't hurt to do this on Win32, but I didn't want
	// to change existing behavior.
#ifdef TARGET_OS_MACOSX
	CreateDirectoryRecursively(m_modsDir);
#endif

    //
    // Load installed mods and set up

    LoadInstalledMods();

    //
    // Set mod path from preferences, unless we are DEMO user

    char authKey[256];
    Authentication_GetKey( authKey );
    bool demoUser = Authentication_IsDemoKey(authKey);

    if( !demoUser )
    {
        SetModPath( g_preferences->GetString( "ModPath" ) );
        UpdateGeographyAffectingMods();
        UpdateStyleAffectingMods();
    }
}


bool ModSystem::PathContainsModContent( const char *_path )
{
    //
    // Check for mod.txt

    char modTxtPath[512];
    sprintf( modTxtPath, "%smod.txt", _path );
    if( DoesFileExist( modTxtPath ) ) return true;

    //
    // Check for data directory with content

    char dataPath[512];
    sprintf( dataPath, "%sdata/", _path );
    if( IsDirectory( dataPath ) ) return true;

    return false;
}


bool ModSystem::FindActualModPath( const char *_basePath, char *_resultPath, int _maxDepth )
{
    //
    // Check if current path contains mod content

    if( PathContainsModContent( _basePath ) )
    {
        strncpy( _resultPath, _basePath, 1023 );
        _resultPath[1023] = '\0';
        return true;
    }

    if( _maxDepth <= 0 ) return false;

    //
    // Search subdirectories recursively
    
    LList<char *> *subDirs = ListSubDirectoryNames( _basePath );
    bool found = false;

    for( int i = 0; i < subDirs->Size(); ++i )
    {
        char *subDir = subDirs->GetData(i);
        
        char subPath[1024];
        sprintf( subPath, "%s%s/", _basePath, subDir );

        if( FindActualModPath( subPath, _resultPath, _maxDepth - 1 ) )
        {
            found = true;
            break;
        }
    }

    subDirs->EmptyAndDeleteArray();
    delete subDirs;

    return found;
}


void ModSystem::LoadInstalledMods()
{
    //
    // Clear out all known mods

    m_mods.EmptyAndDelete();


    //
    // Explore the mods directory, looking for installed mods

    LList<char *> *subDirs = ListSubDirectoryNames( m_modsDir );

    for( int i = 0; i < subDirs->Size(); ++i )
    {
        char *thisSubDir = subDirs->GetData(i);

        char basePath[1024];
        sprintf( basePath, "%s/%s/", m_modsDir, thisSubDir );

        //
        // Search up to 3 levels deep to prevent infinite recursion

        char actualModPath[1024];
        if( !FindActualModPath( basePath, actualModPath, 3 ) )
        {
            AppDebugOut( "Skipping '%s' - no mod content found (searched 3 levels deep)\n", basePath );
            continue;
        }

        InstalledMod *mod = new InstalledMod();
        strcpy( mod->m_name, thisSubDir );
        strncpy( mod->m_path, actualModPath, sizeof(mod->m_path) - 1 );
        mod->m_path[sizeof(mod->m_path) - 1] = '\0';
        sprintf( mod->m_version, "v1.0" );

        LoadModData( mod, mod->m_path );

        if( GetMod( mod->m_name, mod->m_version ) )
        {
            AppDebugOut( "Found installed mod '%s %s' in '%s' %s (DISCARDED:DUPLICATE)\n", 
                        mod->m_name, 
                        mod->m_version, 
                        mod->m_path,
                        mod->m_critical ? "(CRITICAL)" : " " );

            delete mod;
        }
        else
        {
            m_mods.PutData( mod );

            if( strcmp( basePath, actualModPath ) != 0 )
            {
                AppDebugOut( "Found installed mod '%s %s' in '%s' (nested from '%s') %s\n", 
                                mod->m_name, 
                                mod->m_version, 
                                mod->m_path,
                                basePath,
                                mod->m_critical ? "(CRITICAL)" : " " );
            }
            else
            {
                AppDebugOut( "Found installed mod '%s %s' in '%s' %s\n", 
                                mod->m_name, 
                                mod->m_version, 
                                mod->m_path,
                                mod->m_critical ? "(CRITICAL)" : " " );
            }
        }
    }

    subDirs->EmptyAndDeleteArray();
    delete subDirs;
}


LList<char *> *ModSystem::ParseModPath( char *_modPath )
{
    //
    // Tokenize the modPath into strings
    // seperated by semi-colon

    LList<char *> *tokens = new LList<char *>();
    char *currentToken = _modPath;

    while( true )
    {
        char *endPoint = strchr( currentToken, ';' );
        if( !endPoint ) break;

        *endPoint = '\x0';            
        tokens->PutData( currentToken );
        currentToken = endPoint+1;
    }

    //
    // Include any remaining content after the last semicolon

    if( currentToken && *currentToken != '\x0' )
    {
        tokens->PutData( currentToken );
    }

    return tokens;
}


void ModSystem::SetModPath( const char *_modPath )
{
    if( _modPath )
    {
        char modPathCopy[4096];
        strcpy( modPathCopy, _modPath );

        LList<char *> *tokens = ParseModPath( modPathCopy );

        //
        // Try to install each mod from the token list,
        // Assuming its organised as name / version / name / version

        for( int i = tokens->Size()-2; i >= 0; i-=2 )
        {
            char *modName = tokens->GetData(i);
            char *version = tokens->GetData(i+1);

            ActivateMod( modName, version );
        }

        delete tokens;

        //
        // Commit the finished settings

        if( CommitRequired() )
        {
            Commit();
        }
    }
}


void ModSystem::GetCriticalModPath( char *_modPath )
{
    _modPath[0] = '\x0';
    
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( mod->m_active && mod->m_critical )
        {
            strcat( _modPath, mod->m_name );
            strcat( _modPath, ";" );
            strcat( _modPath, mod->m_version );
            strcat( _modPath, ";" );
        }
    }   
}


bool ModSystem::IsCriticalModRunning()
{
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( mod->m_active && mod->m_critical )
        {
            return true;
        }
    }

    return false;
}


bool ModSystem::CanSetModPath( char *_modPath )
{
    char modPathCopy[4096];
    strcpy( modPathCopy, _modPath );

    LList<char *> *tokens = ParseModPath( modPathCopy );

    bool result = true;

    for( int i = 0; i < tokens->Size(); i+=2 )
    {
        char *modName = tokens->GetData(i);
        char *version = tokens->GetData(i+1);

        if( !IsModInstalled( modName, version ) )
        {
            result = false;
            break;
        }
    }

    delete tokens;
    return result;
}


void ModSystem::LoadModData( InstalledMod *_mod, char *_path )
{
    //
    // Try to parse the mod.txt file

    char fullFilename[512];
    sprintf( fullFilename, "%smod.txt", _path );

    if( DoesFileExist( fullFilename ) )
    {
        TextFileReader reader( fullFilename );
        bool lineAvailable = true;

        while( lineAvailable )
        {
            lineAvailable = reader.ReadLine();

            char *fieldName = reader.GetNextToken();
            char *restOfLine = reader.GetRestOfLine();

            if( fieldName && restOfLine )
            {
                if( stricmp( fieldName, "Name" ) == 0 )
                {
                    strncpy( _mod->m_name, reader.GetRestOfLine(), sizeof(_mod->m_name) - 1 );
                    _mod->m_name[sizeof(_mod->m_name) - 1] = '\0';
                }
                else if( stricmp( fieldName, "Version" ) == 0 )
                {
                    strncpy( _mod->m_version, reader.GetRestOfLine(), sizeof(_mod->m_version) - 1 );
                    _mod->m_version[sizeof(_mod->m_version) - 1] = '\0';
                }
                else if( stricmp( fieldName, "Author" ) == 0 )
                {
                    strncpy( _mod->m_author, reader.GetRestOfLine(), sizeof(_mod->m_author) - 1 );
                    _mod->m_author[sizeof(_mod->m_author) - 1] = '\0';
                }
                else if( stricmp( fieldName, "Website" ) == 0 )
                {
                    strncpy( _mod->m_website, reader.GetRestOfLine(), sizeof(_mod->m_website) - 1 );
                    _mod->m_website[sizeof(_mod->m_website) - 1] = '\0';
                }
                else if( stricmp( fieldName, "Comment" ) == 0 )
                {
                    strncpy( _mod->m_comment, reader.GetRestOfLine(), sizeof(_mod->m_comment) - 1 );
                    _mod->m_comment[sizeof(_mod->m_comment) - 1] = '\0';
                }
                else
                {
                    AppDebugOut( "Error parsing '%s' : unrecognised field name '%s'\n", fullFilename, fieldName );
                }
            }
        }        

        StripTrailingWhitespace( _mod->m_name );
        StripTrailingWhitespace( _mod->m_version );
        StripTrailingWhitespace( _mod->m_author );
        StripTrailingWhitespace( _mod->m_website );
        StripTrailingWhitespace( _mod->m_comment );
    }

    //
    // Determine if this is a critical mod or not

    for( int i = 0; i < m_criticalFiles.Size(); ++i )
    {
        char fullPath[512];
        sprintf( fullPath, "%sdata/%s", _path, m_criticalFiles[i] );

        if( DoesFileExist( fullPath ) )
        {
            _mod->m_critical = true;
            break;
        }
    }
}


void ModSystem::ActivateMod( char *_mod, char *_version )
{
    //
    // De-activate any existing versions of this mod

    DeActivateMod( _mod );


    //
    // Activate that version

    bool found = false;

    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( stricmp( mod->m_name, _mod ) == 0 && 
            stricmp( mod->m_version, _version ) == 0 )
        {
            mod->m_active = true;
            if( i != 0 )
            {
                m_mods.RemoveData(i);
                m_mods.PutDataAtStart( mod );
            }
            found = true;
            break;
        }
    }


    if( !found )
    {
        AppDebugOut( "Failed to activate mod '%s %s'\n", _mod, _version );
    }
}


void ModSystem::MoveMod( char *_mod, char *_version, int _move )
{
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( stricmp( mod->m_name, _mod ) == 0 && 
            stricmp( mod->m_version, _version ) == 0 )
        {
            if( m_mods.ValidIndex(i+_move) &&
                m_mods[i+_move]->m_active != mod->m_active )
            {
                // Cant move an active mod into the non-active list
                // and vice versa
                break;
            }

            if( m_mods.ValidIndex(i + _move) )
            {
                m_mods.RemoveData(i);
                m_mods.PutDataAtIndex( mod, i + _move );
            }
            else
            {
                m_mods.RemoveData(i);
                m_mods.PutDataAtIndex( mod, i );
            }

            break;
        }
    }
}


void ModSystem::DeActivateMod( char *_mod )
{
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( stricmp( mod->m_name, _mod ) == 0 && mod->m_active )
        {
            mod->m_active = false;

            //
            // Move the mod so its below all activated modes

            m_mods.RemoveData(i);
            bool added = false;

            for( int j = 0; j < m_mods.Size(); ++j )
            {
                if( !m_mods[j]->m_active )
                {
                    m_mods.PutDataAtIndex( mod, j );
                    added = true;
                    break;
                }
            }

            if( !added )
            {
                m_mods.PutDataAtEnd( mod );
            }

            break;
        }
    }
}


void ModSystem::DeActivateAllMods()
{
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( mod->m_active )
        {
            mod->m_active = false;
        }
    }
}


void ModSystem::DeActivateAllCriticalMods()
{
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( mod->m_active && mod->m_critical )
        {
            mod->m_active = false;
        }
    }
}


InstalledMod *ModSystem::GetMod( char *_mod, char *_version )
{
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( stricmp( mod->m_name, _mod ) == 0 && 
            stricmp( mod->m_version, _version ) == 0 )
        {
            return mod;
        }
    }

    return NULL;
}


bool ModSystem::IsModInstalled ( char *_mod, char *_version )
{
    return( GetMod(_mod, _version) != NULL );
}


bool ModSystem::IsCriticalModPathSet( char *_modPath )
{
    //
    // Build a list of active critical mods

    LList<int> activeMods;

    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( mod->m_active && mod->m_critical )
        {
            activeMods.PutData(i);            
        }
    }   


    //
    // Tokenize the incoming mod path

    char modPathCopy[4096];
    strcpy( modPathCopy, _modPath );

    LList<char *> *tokens = ParseModPath( modPathCopy );


    //
    // Basic check - same number of mods?

    if( tokens->Size()/2 != activeMods.Size() )
    {
        delete tokens;
        return false;
    }


    //
    // Run through the Tokens, looking if they are set

    for( int i = 0; i < tokens->Size(); i+=2 )
    {
        char *desiredModName = tokens->GetData(i);
        char *desiredModVer = tokens->GetData(i+1);

        int actualModIndex = activeMods[ int(i/2) ];
        InstalledMod *actualMod = m_mods[actualModIndex];

        if( strcmp( actualMod->m_name, desiredModName ) != 0 ||
            strcmp( actualMod->m_version, desiredModVer ) != 0 )
        {
            delete tokens;
            return false;
        }
    }


    //
    // Looking good

    delete tokens;
    return true;
}


bool ModSystem::CommitRequired()
{
    //
    // Build a list of Mods that should be running

    LList<char *> modPaths;

    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( mod->m_active )
        {
            modPaths.PutData( mod->m_path );
        }
    }   
    

    //
    // Right number of mods?

    if( modPaths.Size() != g_fileSystem->m_searchPath.Size() )
    {
        return true;
    }


    //
    // Do the mods match?

    for( int i = 0; i < modPaths.Size(); ++i )
    {
        char *modPath = modPaths[i];
        char *installedPath = g_fileSystem->m_searchPath[i];

        if( strcmp( modPath, installedPath ) != 0 )
        {
            return true;
        }
    }

    //
    // Looks like all is well

    return false;
}

void ModSystem::ScanModGraphics()
{
    // Clear existing mod graphics list
    m_modGraphicsFiles.EmptyAndDeleteArray();
    
    // Scan all active mods for graphics files
    for (int i = 0; i < m_mods.Size(); ++i)
    {
        InstalledMod *mod = m_mods[i];
        if (!mod->m_active) continue;
        
        // Build graphics directory path
        char graphicsPath[512];
        sprintf(graphicsPath, "%sdata/graphics/", mod->m_path);
        
        if (IsDirectory(graphicsPath))
        {
            // List all BMP files in mod's graphics directory
            LList<char*> *bmpFiles = ListDirectory(graphicsPath, "*.bmp", false);
            
            for (int j = 0; j < bmpFiles->Size(); ++j)
            {
                char *filename = bmpFiles->GetData(j);
                
                // Store relative path (graphics/filename.bmp)
                char relativePath[256];
                sprintf(relativePath, "graphics/%s", filename);
                
                // Add to mod graphics list if not already present
                bool alreadyExists = false;
                for (int k = 0; k < m_modGraphicsFiles.Size(); ++k)
                {
                    if (strcmp(m_modGraphicsFiles[k], relativePath) == 0)
                    {
                        alreadyExists = true;
                        break;
                    }
                }
                
                if (!alreadyExists)
                {
                    m_modGraphicsFiles.PutData(newStr(relativePath));
                    AppDebugOut("Found mod graphic: %s\n", relativePath);
                }
            }
            
            bmpFiles->EmptyAndDeleteArray();
            delete bmpFiles;
        }
    }
    
    AppDebugOut("Scanned %d mod graphics files\n", m_modGraphicsFiles.Size());
}

bool ModSystem::IsModGraphic(const char* filename)
{
    if (!filename) return false;
    
    // Check if this filename is in our mod graphics list
    for (int i = 0; i < m_modGraphicsFiles.Size(); ++i)
    {
        if (strcmp(m_modGraphicsFiles[i], filename) == 0)
        {
            return true;
        }
    }
    
    return false;
}

void ModSystem::ClearModGraphicsCache()
{
    m_modGraphicsFiles.EmptyAndDeleteArray();
}

bool ModSystem::ModContainsGeographyData(const char* modPath)
{
    if (!modPath) return false;

    char coastlinesPath[512];
    sprintf(coastlinesPath, "%sdata/earth/coastlines.dat", modPath);
    if (DoesFileExist(coastlinesPath)) return true;

    char internationalPath[512];
    sprintf(internationalPath, "%sdata/earth/international.dat", modPath);
    if (DoesFileExist(internationalPath)) return true;
    
    return false;
}

void ModSystem::UpdateGeographyAffectingMods()
{
    m_geographyAffectingMods.EmptyAndDeleteArray();
    
    for (int i = 0; i < m_mods.Size(); ++i)
    {
        InstalledMod *mod = m_mods[i];
        if (mod->m_active && ModContainsGeographyData(mod->m_path))
        {
            m_geographyAffectingMods.PutData(newStr(mod->m_path));
            AppDebugOut("Geography-affecting mod: %s\n", mod->m_name);
        }
    }
}

bool ModSystem::ModContainsStyleData(const char* modPath)
{
    if (!modPath) return false;
    
    char stylePath[512];
    sprintf(stylePath, "%sdata/styles/default.txt", modPath);
    return DoesFileExist(stylePath);
}

void ModSystem::UpdateStyleAffectingMods()
{
    m_styleAffectingMods.EmptyAndDeleteArray();
    
    for (int i = 0; i < m_mods.Size(); ++i)
    {
        InstalledMod *mod = m_mods[i];
        if (mod->m_active && ModContainsStyleData(mod->m_path))
        {
            m_styleAffectingMods.PutData(newStr(mod->m_path));
            AppDebugOut("Style-affecting mod: %s\n", mod->m_name);
        }
    }
}

void ModSystem::Commit()
{
    AppDebugOut( "Committing MOD settings:\n" );

    char authKey[256];
    Authentication_GetKey( authKey );
    bool demoUser = Authentication_IsDemoKey(authKey);
    
    LList<char*> previousGeographyMods;
    for (int i = 0; i < m_geographyAffectingMods.Size(); ++i)
    {
        previousGeographyMods.PutData(newStr(m_geographyAffectingMods[i]));
    }
    
    LList<char*> previousStyleMods;
    for (int i = 0; i < m_styleAffectingMods.Size(); ++i)
    {
        previousStyleMods.PutData(newStr(m_styleAffectingMods[i]));
    }
    
    UpdateGeographyAffectingMods();
    UpdateStyleAffectingMods();
    
    bool geographyChanged = false;
    
    if (previousGeographyMods.Size() != m_geographyAffectingMods.Size())
    {
        geographyChanged = true;
    }
    else
    {
        for (int i = 0; i < previousGeographyMods.Size(); ++i)
        {
            bool found = false;
            for (int j = 0; j < m_geographyAffectingMods.Size(); ++j)
            {
                if (strcmp(previousGeographyMods[i], m_geographyAffectingMods[j]) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                geographyChanged = true;
                break;
            }
        }
    }
    
    bool stylesChanged = false;
    
    if (previousStyleMods.Size() != m_styleAffectingMods.Size())
    {
        stylesChanged = true;
    }
    else
    {
        for (int i = 0; i < previousStyleMods.Size(); ++i)
        {
            bool found = false;
            for (int j = 0; j < m_styleAffectingMods.Size(); ++j)
            {
                if (strcmp(previousStyleMods[i], m_styleAffectingMods[j]) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                stylesChanged = true;
                break;
            }
        }
    }
    
    previousGeographyMods.EmptyAndDeleteArray();
    previousStyleMods.EmptyAndDeleteArray();
    
    ClearModGraphicsCache();

    g_fileSystem->ClearSearchPath();

    //
    // Load base game default.txt FIRST before mod paths are added

    if( g_styleTable )
    {
        g_styleTable->Load( "default.txt" );
    }

    // This ensures all base styles exist in memory
    // Mod styles will merge on top of these later

    char fullModPath[4096];
    char modDescription[8192];
    char temp[1024];

    fullModPath[0] = '\x0';
    modDescription[0] = '\x0';
    int numActivated = 0;

    if( g_languageTable && g_windowManager )
    {
        strcat( modDescription, LANGUAGEPHRASE("dialog_mod_commit_mod_path") );
    }

    if( !demoUser )
    {
        for( int i = 0; i < m_mods.Size(); ++i )
        {
            InstalledMod *mod = m_mods[i];

            if( mod->m_active )
            {
                g_fileSystem->AddSearchPath( mod->m_path );

                AppDebugOut( "Activated mod '%s %s'\n", mod->m_name, mod->m_version );
                                
                sprintf( temp, "%s;%s;", mod->m_name, mod->m_version );
                strcat( fullModPath, temp );

                if( g_languageTable && g_windowManager )
                {
                    strcpy( temp, LANGUAGEPHRASE("dialog_mod_commit_activating_mod") );
                    LPREPLACESTRINGFLAG( 'N', mod->m_name, temp );
                    LPREPLACESTRINGFLAG( 'V', mod->m_version, temp );
                    strcat( modDescription, temp );
                }

                numActivated++;
            }
        }   
    }

    ScanModGraphics();

    if( g_languageTable && g_windowManager )
    {
        if( numActivated == 0 )
        {
            strcpy( temp, LANGUAGEPHRASE("dialog_mod_commit_all_mods_off") );
            strcat( modDescription, temp );
        }
    }

    //
    // Open up a window to show MODs are being loaded

    MessageDialog *msgDialog = NULL;

    if( g_languageTable && g_windowManager )
    {
        msgDialog = new MessageDialog( "LOADING MODS...", modDescription, false, "dialog_mod_commit_title", true );    
        EclRegisterWindow( msgDialog );
        g_app->Render();
    }

    //
    // Restart everything

    if( g_resource )
    {
        g_languageTable->LoadLanguages();
        g_languageTable->LoadCurrentLanguage();

        //
        // Protect VBOs from InvalidateAllVBOs if geography hasnt changed
        // since resource restart will invalidate all VBOs.
        // But dont protect VBOs if styles changed
        
        if (!geographyChanged)
        {
            g_renderer3dvbo->Protect3DVBO("Starfield");
            g_renderer3dvbo->Protect3DVBO("CullingSphere");

            if (!stylesChanged)
            {
                g_renderer2dvbo->ProtectVBO("MapCoastlines");
                g_renderer2dvbo->ProtectVBO("MapBorders");
                g_renderer3dvbo->Protect3DVBO("GlobeCoastlines");
                g_renderer3dvbo->Protect3DVBO("GlobeBorders");
                g_renderer3dvbo->Protect3DVBO("GlobeGridlines");
            }
        }

        g_resource->Restart();
        
        g_renderer2dvbo->ClearVBOProtection();
        g_renderer3dvbo->Clear3DVBOProtection();
        
        g_app->InitFonts();
        g_app->GetMapRenderer()->Init();
        g_app->GetGlobeRenderer()->Init();

        if (geographyChanged)
        {
            g_app->GetEarthData()->LoadCoastlines();
            g_app->GetEarthData()->LoadBorders();
            g_app->GetEarthData()->CalculateAndSetBufferSizes();
        }

        if( !g_app->m_gameRunning )
        {
            // It's only safe to do this when a game is NOT running
            // Because the Cities stored here are used in World
            g_app->GetEarthData()->LoadCities();
        }

#ifdef TOGGLE_SOUND
        g_soundSystem->EnableCallback(false);
        g_soundSystem->m_sounds.EmptyAndDelete();
        g_soundSystem->m_blueprints.ClearAll();
        g_soundSampleBank->EmptyCache();
        g_soundSystem->m_blueprints.LoadEffects();
        g_soundSystem->m_blueprints.LoadBlueprints();
        g_soundSystem->PropagateBlueprints(true);
        g_soundSystem->EnableCallback(true);
#endif


        if( g_styleTable )
        {
            g_app->MergeStyles();

            //
            // If styles changed, invalidate VBOs so they rebuild with new colors

            if( stylesChanged )
            {
                g_renderer2dvbo->InvalidateCachedVBO("MapCoastlines");
                g_renderer2dvbo->InvalidateCachedVBO("MapBorders");
                g_renderer3dvbo->InvalidateCached3DVBO("GlobeCoastlines");
                g_renderer3dvbo->InvalidateCached3DVBO("GlobeBorders");
                g_renderer3dvbo->InvalidateCached3DVBO("GlobeGridlines");
            }
        }
    }
        
    if( msgDialog )
    {
        EclRemoveWindow( msgDialog->m_name );
    }

    AppDebugOut( "%d MODs activated\n", numActivated );

    g_preferences->SetString( "ModPath", fullModPath );
    g_preferences->Save();
}



// ============================================================================



InstalledMod::InstalledMod()
:   m_active(false),
    m_critical(false)
{
    sprintf( m_name,    "unknown" );
    sprintf( m_version, "unknown" );
    sprintf( m_path,    "unknown" );
    sprintf( m_author,  "unknown" );
    sprintf( m_website, "unknown" );
    sprintf( m_comment, "unknown" );
}
