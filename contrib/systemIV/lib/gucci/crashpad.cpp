#include "systemiv.h"

#include "crashpad.h"

#include "lib/debug/debug_utils.h"
#include "lib/preferences.h"

#ifdef USE_CRASHREPORTING

using namespace crashpad;

static CrashpadClient g_crashpadClient;

bool InitializeCrashpad( const char *apiUrl )
{
	//
	// API URL, but can literally be anything

	const char *prefsUrl = nullptr;
	if ( g_preferences )
	{
		prefsUrl = g_preferences->GetString( PREFS_CRASHPAD_URL, 0 );
	}


	const char *finalUrl = apiUrl;
	if ( !finalUrl || !finalUrl[0] )
	{
		finalUrl = prefsUrl;
	}

	if ( !finalUrl || !finalUrl[0] )
	{
		AppDebugOut( "Crashpad: No CrashpadURL configured in preferences\n" );
		finalUrl = ""; 
	}

	apiUrl = finalUrl;

	base::FilePath exeFilePath;
	base::FilePath exeDir;

#ifdef TARGET_MSVC
	wchar_t exePathW[MAX_PATH] = {};
	DWORD len = GetModuleFileNameW( nullptr, exePathW, MAX_PATH );
	if ( len == 0 || len == MAX_PATH )
	{
		AppDebugOut( "Crashpad: GetModuleFileNameW failed, not enabling crash reporting\n" );
		return false;
	}

	exeFilePath = base::FilePath( exePathW );
	exeDir = exeFilePath.DirName();

	//
	// crashpad_handler.exe must be in the same directory as defcon.exe

	base::FilePath handlerPath =
		exeDir.Append( FILE_PATH_LITERAL( "crashpad_handler.exe" ) );

#elif defined( TARGET_OS_LINUX )

	char exePath[PATH_MAX] = {};
	ssize_t len = readlink( "/proc/self/exe", exePath, PATH_MAX - 1 );

	if ( len == -1 || len >= PATH_MAX - 1 )
	{
		AppDebugOut( "Crashpad: readlink(/proc/self/exe) failed, not enabling crash reporting\n" );
		return false;
	}
	exePath[len] = '\0';

	exeFilePath = base::FilePath( exePath );
	exeDir = exeFilePath.DirName();

	//
	// crashpad_handler must be in the same directory as defcon

	base::FilePath handlerPath =
		exeDir.Append( FILE_PATH_LITERAL( "crashpad_handler" ) );

#endif

	//
	// Crashpad database directory, minidumps and metadata

	base::FilePath dbPath =
		exeDir.Append( FILE_PATH_LITERAL( "crashpad_db" ) );

	//
	// Not used and wont ever be used

	base::FilePath metricsPath;

	//
	// Crash description, identifies crashes in the console

	std::map<std::string, std::string> annotations;
	annotations["Version"] = GetAppVersion();
	annotations["InternalVersion"] = GetRealVersion();
	annotations["Platform"] = GetAppSystem();

	//
	// Dont use compression, reduces API complexity

	std::vector<std::string> arguments;

	arguments.push_back( "--no-upload-gzip" );

	//
	// Add log files to attachments, they could be useful

	std::vector<base::FilePath> attachments;

#ifdef TARGET_MSVC

	//
	// On Windows, both files are in the exe directory

	attachments.push_back( exeDir.Append( FILE_PATH_LITERAL( "debug.txt" ) ) );
	attachments.push_back( exeDir.Append( FILE_PATH_LITERAL( "blackbox.txt" ) ) );

#elif defined( TARGET_OS_LINUX )

	//
	// On Linux, debug.txt is in ~/.defcon/

	const char *home = getenv( "HOME" );
	if ( home && home[0] )
	{
		std::string defconDir = std::string( home ) + "/.defcon/";
		base::FilePath defconPath( defconDir );
		attachments.push_back( defconPath.Append( FILE_PATH_LITERAL( "debug.txt" ) ) );
	}

#endif

	const bool restartable = true;
	const bool asynchronous_start = true;

	bool startOk = g_crashpadClient.StartHandler(
		handlerPath, // crashpad_handler binary
		dbPath,		 // database path
		metricsPath, // not used
		apiUrl,		 // upload URL
		annotations, // default annotations
		arguments,	 // args
		restartable, // restart handler if it crashes
		asynchronous_start,
		attachments // files to attach
	);

	if ( !startOk )
	{
		AppDebugOut( "Crashpad: StartHandler failed, not enabling crash reporting\n" );
		return false;
	}

	AppDebugOut( "Crashpad: Handler started successfully\n" );

	//
	// Make sure uploads are enabled in the local DB

	std::unique_ptr<CrashReportDatabase> database =
		CrashReportDatabase::Initialize( dbPath );

	if ( database && database->GetSettings() )
	{
		database->GetSettings()->SetUploadsEnabled( true );
	}

	return true;
}

#endif // USE_CRASHREPORTING