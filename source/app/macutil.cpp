#include "lib/universal_include.h"

#include "macutil.h"

char *GetLocale()
{
	char *locale = new char[64];
	CFStringGetCString(CFLocaleGetIdentifier(CFLocaleCopyCurrent()), locale, 64, kCFStringEncodingASCII);
	return locale;
}

char *GetBundleResourcePath( char *_resourceName )
{
	CFStringRef resourceName;
	CFURLRef resourceURL;
	CFStringRef resourcePath;
	char *cPath = NULL;
	unsigned cPathLen;
	
	resourceName = CFStringCreateWithCString(NULL, _resourceName, kCFStringEncodingUTF8);
	resourceURL = CFBundleCopyResourceURL(CFBundleGetMainBundle(), resourceName, NULL, NULL);
	if ( resourceURL )
	{
		resourcePath = CFURLCopyFileSystemPath(resourceURL, kCFURLPOSIXPathStyle);
		
		cPathLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(resourcePath), kCFStringEncodingUTF8);
		cPath = new char [ cPathLen ];
		CFStringGetCString(resourcePath, cPath, cPathLen, kCFStringEncodingUTF8);
		
		CFRelease(resourcePath);
	}
	CFRelease(resourceName);
	
	return cPath;
}

bool OpenBundleResource( char *_name, char *_type )
{
	CFStringRef name, type;
	CFURLRef resourceURL;
	bool succeeded;
	
	name = CFStringCreateWithCString(NULL, _name, kCFStringEncodingUTF8);
	type = CFStringCreateWithCString(NULL, _type, kCFStringEncodingUTF8);
	resourceURL = CFBundleCopyResourceURL(CFBundleGetMainBundle(), name, type, NULL);
	CFRelease(name);
	CFRelease(type);
	
	if ( resourceURL )
	{
		succeeded = ( LSOpenCFURLRef(resourceURL, NULL) ) == noErr;
		CFRelease(resourceURL);
	}
	else
	{
		succeeded = false;
	}
	
	return succeeded;
}
