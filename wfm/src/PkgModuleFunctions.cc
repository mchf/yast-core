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

   File:	PkgModuleFunctions.cc

   Author:	Klaus Kaempf <kkaempf@suse.de>
   Maintainer:  Klaus Kaempf <kkaempf@suse.de>

   Purpose:	PkgModuleFunctions constructor, destructor
/-*/


#include <ycp/y2log.h>
#include <PkgModuleFunctions.h>

#include <y2pm/InstSrcDescr.h>
#include <y2pm/InstSrcManager.h>
#include <y2pm/InstSrcDescr.h>
#include <ycp/YCPError.h>
#include <ycp/YCPInteger.h>
#include <ycp/YCPString.h>
#include <ycp/YCPBoolean.h>
#include <ycp/YCPMap.h>

/**
 * Constructor.
 */
PkgModuleFunctions::PkgModuleFunctions (YCPInterpreter *wfmInterpreter)
    : _wfm (wfmInterpreter)
    , _first_free_source_slot(0)
    , _cache_started(false)
{
    _y2pm.packageManager();
}

/**
 * Destructor.
 */
PkgModuleFunctions::~PkgModuleFunctions ()
{
}


// ------------------------------------------------------------------
// general

/**
 * @builtin Pkg::InstSysMode () -> void
 *
 * Set packagemanager to "inst-sys" mode
 * - dont use local caches (ramdisk!)
 *
 * !!!!!!!!!! CAUTION !!!!!!!!!!!
 * Can only be called ONCE
 * MUST be called before any other function
 * !!!!!!!!!! CAUTION !!!!!!!!!!!
 */
YCPValue
PkgModuleFunctions::InstSysMode (YCPList args)
{
    _y2pm.setNotRunningFromSystem();
    return YCPVoid();
}


/**
 * @builtin Pkg::SetLocale (string locale) -> void
 *
 * set the given locale as the "preferred" locale
 */
YCPValue
PkgModuleFunctions::SetLocale (YCPList args)
{
    if ((args->size() != 1)
	|| !(args->value(0)->isString()))
    {
	return YCPError ("Bad args to Pkg::SetLocale");
    }
    LangCode langcode(args->value(0)->asString()->value());
    _y2pm.setPreferredLocale (langcode);
    return YCPVoid();
}

/**
 * @builtin Pkg::GetLocale () -> string locale
 *
 * get the currently preferred locale
 */
YCPValue
PkgModuleFunctions::GetLocale (YCPList args)
{
    return YCPString ((const std::string &)(_y2pm.getPreferredLocale()));
}


/**
 * @builtin Pkg::SetAdditionalLocales (list of string) -> void
 *
 * set list of 
 */
YCPValue
PkgModuleFunctions::SetAdditionalLocales (YCPList args)
{
    if ((args->size() != 1)
	|| !(args->value(0)->isList()))
    {
	return YCPError ("Bad args to Pkg::SetAdditionalLocales");
    }
    std::list<LangCode> langcodelist;
    int i = 0;
    YCPList langycplist = args->value(0)->asList();
    while (i < langycplist->size())
    {
	if (langycplist->value (i)->isString())
	{
	    langcodelist.push_back (LangCode (langycplist->value (i)->asString()->value()));
	}
	else
	{
	    y2error ("Pkg::SetAdditionalLocales ([...,%s,...]) not string", langycplist->value (i)->toString().c_str());
	}
	i++;
    }
    _y2pm.setRequestedLocales (langcodelist);
    return YCPVoid();
}

/**
 * @builtin Pkg::GetAdditionalLocales -> list
 *
 * return list of additional locales
 */
YCPValue
PkgModuleFunctions::GetAdditionalLocales (YCPList args)
{
    YCPList langycplist;
    const std::list<LangCode> langcodelist = _y2pm.getRequestedLocales();
    for (std::list<LangCode>::const_iterator it = langcodelist.begin();
	 it != langcodelist.end(); ++it)
    {
	langycplist->add (YCPString ((const std::string &)(*it)));
    }
    return langycplist;
}


/**
 * @builtin Pkg::Error
 *
 * get current error as string
 */
YCPValue
PkgModuleFunctions::Error (YCPList args)
{
    return YCPString (_last_error.errstr());
}


/** ------------------------
   convert constInstSrcDescrPtr to YCPMap
 */

YCPMap
PkgModuleFunctions::Descr2Map (constInstSrcDescrPtr descr)
{
    YCPMap map;

    map->add (YCPString ("product"), YCPString (PkgNameEd::toString(descr->content_product())));
    map->add (YCPString ("vendor"), YCPString (descr->content_vendor()));
    map->add (YCPString ("requires"), YCPString (descr->content_requires().asString()));
    return map;
}


