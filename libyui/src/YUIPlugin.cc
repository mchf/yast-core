/*---------------------------------------------------------------------\
|								       |
|		       __   __	  ____ _____ ____		       |
|		       \ \ / /_ _/ ___|_   _|___ \		       |
|			\ V / _` \___ \ | |   __) |		       |
|			 | | (_| |___) || |  / __/		       |
|			 |_|\__,_|____/ |_| |_____|		       |
|								       |
|				core system			       |
|							 (C) SuSE GmbH |
\----------------------------------------------------------------------/

  File:		YUIPlugin.h

  Author:	Stefan Hundhammer <sh@suse.de>

/-*/


#include <dlfcn.h>

#define y2log_component "ui"
#include <ycp/y2log.h>
#include "YUIPlugin.h"


#define PLUGIN_PREFIX "libpy2"
#define PLUGIN_SUFFIX ".so.2"


using std::string;


YUIPlugin::YUIPlugin( const char * pluginLibBaseName )
{
    _pluginLibBaseName = string( pluginLibBaseName );

    string pluginFilename = pluginLibFullPath();

    _pluginLibHandle = dlopen( pluginFilename.c_str(),
			       RTLD_LAZY | RTLD_GLOBAL);

    if ( ! _pluginLibHandle )
    {
	_errorMsg = dlerror();
	y2error( "Could not load UI plugin \"%s\": %s",
		 pluginLibBaseName, _errorMsg.c_str() );
    }
}


YUIPlugin::~YUIPlugin()
{
    if ( _pluginLibHandle )
	dlclose( _pluginLibHandle );
}


string
YUIPlugin::pluginLibFullPath() const
{
    string fullPath;

    fullPath.append( PLUGINDIR "/" ); // from -DPLUGINDIR in Makefile.am
    fullPath.append( PLUGIN_PREFIX );
    fullPath.append( _pluginLibBaseName );
    fullPath.append( PLUGIN_SUFFIX );

    return fullPath;
}


void * YUIPlugin::locateSymbol( const char * symbol )
{
    if ( ! _pluginLibHandle )
	return 0;
    
    void * addr = dlsym( _pluginLibHandle, symbol  );

    if ( ! addr )
    {
	y2error( "Could not locate symbol \"%s\" in %s",
		 symbol, pluginLibFullPath().c_str() );
    }

    return addr;
}


bool YUIPlugin::error() const
{
    return _pluginLibHandle == 0;
}


bool YUIPlugin::success() const
{
    return _pluginLibHandle != 0;
}


string YUIPlugin::errorMsg() const
{
    return _errorMsg;
}
