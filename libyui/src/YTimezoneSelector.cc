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

  File:		YTimezoneSelector.cc

  Author:	Stephan Kulow <coolo@suse.de>

/-*/


#define y2log_component "ui"
#include <ycp/y2log.h>

#include "YUISymbols.h"
#include "YTimezoneSelector.h"



YTimezoneSelector::YTimezoneSelector( YWidget *		parent,
                                      const string &pixmap,
                                      const map<string, string> &timezones )
    : YWidget( parent )
{
}


YTimezoneSelector::~YTimezoneSelector()
{
    // NOP
}


const YPropertySet &
YTimezoneSelector::propertySet()
{
    static YPropertySet propSet;

    if ( propSet.isEmpty() )
    {
        propSet.add( YProperty( YUIProperty_Value, YStringProperty ) );
	propSet.add( YWidget::propertySet() );
    }

    return propSet;
}


bool
YTimezoneSelector::setProperty( const string & propertyName, const YPropertyValue & val )
{
    propertySet().check( propertyName, val.type() ); // throws exceptions if not found or type mismatch

    if ( propertyName == YUIProperty_Value )
    {
        setCurrentZone( val.stringVal() );
        return true; // success -- no special handling necessary
    }
    else
    {
	return YWidget::setProperty( propertyName, val );
    }
}


YPropertyValue
YTimezoneSelector::getProperty( const string & propertyName )
{
    propertySet().check( propertyName ); // throws exceptions if not found

    if ( propertyName == YUIProperty_Value ) 	return YPropertyValue( currentZone() );
    else
    {
	return YWidget::getProperty( propertyName );
    }
}

