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

   File:	PkgModuleFunctionsPackage.cc

   Author:	Klaus Kaempf <kkaempf@suse.de>
   Maintainer:  Klaus Kaempf <kkaempf@suse.de>

   Purpose:	Access to packagemanager
		Handles package related Pkg::function (list_of_arguments) calls
		from WFMInterpreter.
/-*/

#include <iostream>
#include <fstream>
#include <utility>

#include <ycp/y2log.h>
#include <PkgModuleFunctions.h>

#include <y2util/Url.h>
#include <y2util/FSize.h>
#include <y2pm/InstData.h>
#include <y2pm/InstSrcDescr.h>
#include <y2pm/InstTarget.h>
#include <y2pm/PMObject.h>
#include <y2pm/PMSelectable.h>
#include <y2pm/PMPackageManager.h>
#include <y2pm/PMSelectionManager.h>

#include <ycp/YCPVoid.h>
#include <ycp/YCPBoolean.h>
#include <ycp/YCPInteger.h>
#include <ycp/YCPSymbol.h>
#include <ycp/YCPString.h>
#include <ycp/YCPList.h>
#include <ycp/YCPMap.h>

using std::string;

/**
 * helper function, get selectable by name
 */

PMSelectablePtr
PkgModuleFunctions::getPackageSelectable (const std::string& name)
{
    PMSelectablePtr selectable;
    if (!name.empty())
        selectable = _y2pm.packageManager().getItem(name);
    return selectable;
}


/**
 * helper function, get the current object of a selectable
 */

static PMPackagePtr
getTheObject (PMSelectablePtr selectable)
{
    PMPackagePtr package;
    if (selectable)
    {
        package = selectable->theObject();
        if (!package)
        {
            y2error ("Package '%s' not found", ((const std::string&)(selectable->name())).c_str());
        }
    }
    return package;
}


// ------------------------
/**
 *  @builtin Pkg::PkgMediaNames () -> [ "source_1_name", "source_2_name", ...]
 *    return names of sources in installation order
 *  list<string>
 */
YCPValue
PkgModuleFunctions::PkgMediaNames ()
{
    // get sources in installation order
    InstSrcManager::ISrcIdList inst_order( _y2pm.instSrcManager().instOrderSources() );

    YCPList ycpnames;

    for (InstSrcManager::ISrcIdList::const_iterator it = inst_order.begin(); it != inst_order.end(); ++it)
    {
	ycpnames->add (YCPString ((const std::string &)((*it)->descr()->content_product().name)));
    }
    return ycpnames;
}

// ------------------------
/**
 *  @builtin Pkg::PkgMediaSizes () ->
 *    [ [src1_media_1_size, src1_media_2_size, ...], [src2_media_1_size, src2_media_2_size, ...], ...]
 *    return cumulated sizes (in bytes !) to be installed from different sources and media
 *
 *   Returns the install size, not the archivesize !!
 *  list <list <integer> >
 */
YCPValue
PkgModuleFunctions::PkgMediaSizes ()
{
    // get sources in installation order
    InstSrcManager::ISrcIdList inst_order( _y2pm.instSrcManager().instOrderSources() );

    // compute max number of media across all sources
    // need a source rank map here since the package reveals the source rank as
    // a nonambiguous id for its source.
    // we will later re-map it back to the installation order. This is why InstSrcManager::ISrcId
    // is included.

    // map of < source rank, < src id, vector of media sizes >

    map <unsigned int, pair <InstSrcManager::ISrcId, vector<FSize> > > mediasizes;

    for (InstSrcManager::ISrcIdList::const_iterator it = inst_order.begin(); it != inst_order.end(); ++it)
    {
        vector<FSize> vf ((*it)->descr()->media_count(), FSize (0));
        mediasizes[(*it)->descr()->default_rank()] = std::make_pair (*it, vf);
    }

    // scan over all packages

    for (PMPackageManager::PMSelectableVec::const_iterator pkg = _y2pm.packageManager().begin();
         pkg != _y2pm.packageManager().end(); ++pkg)
    {
        if (!((*pkg)->to_install()))
            continue;
        PMPackagePtr package = (*pkg)->candidateObj();
        if (!package)
        {
            continue;
        }
        map <unsigned int, pair <InstSrcManager::ISrcId, vector<FSize> > >::iterator mediapair = mediasizes.find (package->instSrcRank());
        if (mediapair == mediasizes.end())
        {
            y2error ("Unknown rank %d for '%s'", package->instSrcRank(), ((std::string)(*pkg)->name()).c_str());
            continue;
        }
        unsigned int medianr = package->medianr();
        FSize size = package->size();
        if (medianr > mediapair->second.second.size())
        {
            y2error ("resize needed %d", medianr);
            mediapair->second.second.resize (medianr);
        }
        mediapair->second.second[medianr-1] += size;
    }

    // now convert back to list of lists in installation order

    YCPList ycpsizes;

    for (InstSrcManager::ISrcIdList::const_iterator it = inst_order.begin(); it != inst_order.end(); ++it)
    {
        for (map <unsigned int, pair <InstSrcManager::ISrcId, vector<FSize> > >::iterator mediapair = mediasizes.begin();
             mediapair != mediasizes.end(); ++mediapair)
        {
            if (mediapair->second.first == *it)
            {
                YCPList msizes;

                for (unsigned int i = 0; i < mediapair->second.second.size(); ++i)
                {
                    msizes->add (YCPInteger (((long long)(mediapair->second.second[i]))));
                }
                ycpsizes->add (msizes);
            }
        }
    }

    return ycpsizes;
}

// ------------------------
/**
   @builtin Pkg::IsProvided (string tag) -> boolean

   returns a 'true' if the tag is provided in the installed system

   tag can be a package name, a string from requires/provides
   or a file name (since a package implictly provides all its files)
*/
YCPValue
PkgModuleFunctions::IsProvided (const YCPString& tag)
{
    std::string name = tag->value ();
    if (name.empty())
	return YCPBoolean (false);

    // check package name first, then tag, then file
    if (!_y2pm.instTarget().hasPackage (PkgName (name)))		// package
    {
	if (!_y2pm.instTarget().hasProvides (name))			// provided tag
	{
	    return YCPBoolean (_y2pm.instTarget().hasFile (name));	// file name
	}
    }
    return YCPBoolean (true);
}


// ------------------------
/**
   @builtin Pkg::IsSelected (string tag) -> boolean

   returns a 'true' if the tag is selected for installation

   tag can be a package name, a string from requires/provides
   or a file name (since a package implictly provides all its files)
*/
YCPValue
PkgModuleFunctions::IsSelected (const YCPString& tag)
{
    std::string name = tag->value ();
    if (name.empty())
	return YCPBoolean (false);

    PMSelectablePtr selectable = getPackageSelectable (name);
    if (!selectable)
    {
	selectable = PkgModuleFunctions::WhoProvidesString (name);
	if (!selectable)
	    return YCPBoolean (false);
    }
    if (selectable->to_install())
    {
	return YCPBoolean (true);
    }
    return YCPBoolean (false);
}

// ------------------------
/**
   @builtin Pkg::IsAvailable (string tag) -> boolean

   returns a 'true' if the tag is available on any of the currently
   active installation sources. (i.e. it is installable)

   tag can be a package name, a string from requires/provides
   or a file name (since a package implictly provides all its files)
*/
YCPValue
PkgModuleFunctions::IsAvailable (const YCPString& tag)
{
    std::string name = tag->value ();
    if (name.empty())
	return YCPBoolean (false);

    PMSelectablePtr selectable = getPackageSelectable (name);
    if (!selectable)
    {
        selectable = PkgModuleFunctions::WhoProvidesString (name);
        if (!selectable)
            return YCPBoolean (false);
    }
    if (selectable->candidateObj() == 0)
        return YCPBoolean (false);
    return YCPBoolean (true);
}


/**
 * helper function, find a package which provides tag (as a
 *   provided tag or a provided file)
 *
 */

PMSelectablePtr
PkgModuleFunctions::WhoProvidesString (std::string tag)
{
    for (PMPackageManager::PMSelectableVec::const_iterator sel = _y2pm.packageManager().begin();
	 sel != _y2pm.packageManager().end(); ++sel)
    {
	if ((*sel)->has_installed()
	    && (*sel)->installedObj()->doesProvide (PkgRelation (PkgName (tag))))
	{
	    return *sel;
	}
	else if ((*sel)->has_candidate()
	    && (*sel)->candidateObj()->doesProvide (PkgRelation (PkgName (tag))))
	{
	    return *sel;
	}
    }
    return 0;
}


/**
 * helper function, install a package which provides tag (as a
 *   package name, a provided tag, or a provided file
 *
 */

bool
PkgModuleFunctions::DoProvideString (std::string name)
{
    PMSelectablePtr selectable = getPackageSelectable (name);		// by name
    if (!selectable)
    {
	selectable = WhoProvidesString (name);				// by tag
	if (!selectable)
	{
	    return false;						// no package found
	}
    }
    selectable->appl_set_install();
    return true;
}

// ------------------------
/**
   @builtin Pkg::DoProvide (list tags) -> $["failed1":"reason", ...]

   Provides (read: installs) a list of tags to the system

   tag can be a package name, a string from requires/provides
   or a file name (since a package implictly provides all its files)

   returns a map of tag,reason pairs if tags could not be provided.
   Usually this map should be empty (all required packages are
   installed)
   If tags could not be provided (due to package install failures or
   conflicts), the tag is listed as a key and the value describes
   the reason for the failure (as an already translated string).
*/
YCPValue
PkgModuleFunctions::DoProvide (const YCPList& tags)
{
    YCPMap ret;
    if (tags->size() > 0)
    {
        for (int i = 0; i < tags->size(); ++i)
        {
            if (tags->value(i)->isString())
            {
                DoProvideString (tags->value(i)->asString()->value());
            }
            else
            {
                y2error ("Pkg::DoProvide not string '%s'", tags->value(i)->toString().c_str());
            }
        }
    }
    return ret;
}

// internal
bool
PkgModuleFunctions::DoRemoveString (std::string name)
{
    PMSelectablePtr selectable = getPackageSelectable(name);
    if (!selectable)
    {
	y2error ("Remove: package '%s' not found", name.c_str());
	return false;
    }
    selectable->appl_set_delete();
    return true;
}

// ------------------------
/**
   @builtin Pkg::DoRemove (list tags) -> ["failed1", ...]

   Removes a list of tags from the system

   tag can be a package name, a string from requires/provides
   or a file name (since a package implictly provides all its files)

   returns a map of tag,reason pairs if tags could not be removed.
   Usually this map should be empty (all required packages are
   removed)
   If a tag could not be removed (because other packages still
   require it), the tag is listed as a key and the value describes
   the reason for the failure (as an already translated string).
*/
YCPValue
PkgModuleFunctions::DoRemove (const YCPList& tags)
{
    _y2pm.packageManager ();

    YCPMap ret;
    if (tags->size() > 0)
    {
	for (int i = 0; i < tags->size(); ++i)
	{
	    if (tags->value(i)->isString())
	    {
		DoRemoveString (tags->value(i)->asString()->value());
	    }
	    else
	    {
		y2error ("Pkg::DoRemove not string '%s'", tags->value(i)->toString().c_str());
	    }
	}
    }
    return ret;
}

// ------------------------
/**
   @builtin Pkg::PkgSummary (string package) -> "This is a nice package"

   Get summary (aka label) of a package

*/
YCPValue
PkgModuleFunctions::PkgSummary (const YCPString& p)
{
    PMPackagePtr package = getTheObject (getPackageSelectable (p->value ()));
    if (!package)
    {
        return YCPVoid();
    }

    return YCPString (package->summary());

}

// ------------------------
/**
   @builtin Pkg::PkgVersion (string package) -> "1.42-39"

   Get version (better: edition) of a package

*/
YCPValue
PkgModuleFunctions::PkgVersion (const YCPString& p)
{
    PMPackagePtr package = getTheObject (getPackageSelectable (p->value ()));
    if (!package)
    {
        return YCPVoid();
    }

    return YCPString (package->edition().asString());

}

// ------------------------
/**
   @builtin Pkg::PkgSize (string package) -> 12345678

   Get (installed) size of a package

*/
YCPValue
PkgModuleFunctions::PkgSize (const YCPString& p)
{
    PMPackagePtr package = getTheObject (getPackageSelectable (p->value ()));
    if (!package)
    {
	return YCPVoid();
    }

    return YCPInteger ((long long)(package->size()));
}

// ------------------------
/**
   @builtin Pkg::PkgGroup (string package) -> string

   Get rpm group of a package

*/
YCPValue
PkgModuleFunctions::PkgGroup (const YCPString& p)
{
    PMPackagePtr package = getTheObject (getPackageSelectable (p->value ()));
    if (!package)
    {
	return YCPVoid();
    }

    return YCPString (package->group());
}

// ------------------------
/**
 * @builtin Pkg::PkgProperties (string package) -> map
 * @param package name
 *
 * @return Data about package location, source and which
 *  media contains the package
 *
 * <TABLE>
 * <TR><TD>$[<TD>"srcid"  <TD>: YCPInteger
 * <TR><TD>,<TD>"location"  <TD>: YCPString
 * <TR><TD>,<TD>"medianr"  <TD>: YCPInteger
 * <TR><TD>,<TD>"arch"       <TD>: YCPString
 * <TR><TD>];
 * </TABLE>
 *
 */
YCPValue
PkgModuleFunctions::PkgProperties (const YCPString& p)
{
    PMPackagePtr package = getTheObject (getPackageSelectable (p->value()));
    if (!package)
    {
	return YCPVoid();
    }

    constInstSrcPtr source( package->source() );

    YCPMap data;
    data->add( YCPString("srcid"), YCPInteger(source->srcID() ));
    data->add( YCPString("location"), YCPString (package->location() ));
    data->add( YCPString("medianr"), YCPInteger (package->medianr() ));
    data->add( YCPString("arch"), YCPString (package->arch() ));

    return data;
}

// ------------------------
/**
   @builtin Pkg::PkgLocation (string package) -> string

   Get file location of a package in the source

*/
YCPValue
PkgModuleFunctions::PkgLocation (const YCPString& p)
{
    PMPackagePtr package = getTheObject (getPackageSelectable (p->value()));
    if (!package)
    {
       return YCPVoid();
    }

    return YCPString (package->location());
}

// ------------------------
/**
   @builtin Pkg::SaveState() -> bool

   save the current package selection status for later
   retrieval via Pkg::RestoreState()

*/
YCPValue
PkgModuleFunctions::SaveState ()
{
    _y2pm.packageSelectionSaveState();
    return YCPBoolean (true);
}

// ------------------------
/**
   @builtin Pkg::RestoreState(bool check_only = false) -> bool

   restore the package selection status from a former
   call to Pkg::SaveState()
   Returns false if there is no saved state (no Pkg::SaveState()
   called before)

   If called with argument (true), it only checks the saved
   against the current status and returns true if they differ.

*/
YCPValue
PkgModuleFunctions::RestoreState (const YCPBoolean& ch)
{
    if (!ch.isNull () && ch->value () == true)
    {
	return YCPBoolean (_y2pm.packageSelectionDiffState());
    }
    return YCPBoolean (_y2pm.packageSelectionRestoreState());
}

// ------------------------
/**
   @builtin Pkg::ClearSaveState() -> bool

   clear a saved state (to reduce memory consumption)

*/
YCPValue
PkgModuleFunctions::ClearSaveState ()
{
    _y2pm.packageSelectionClearSaveState();
    return YCPBoolean (true);
}

// ------------------------
/**
   @builtin Pkg::IsManualSelection () -> bool

   return true if the original list of packages (since the
   last Pkg::SetSelection()) was changed.

*/
YCPValue
PkgModuleFunctions::IsManualSelection ()
{
    return YCPBoolean (_y2pm.packageManager().anythingByUser()
			|| _y2pm.selectionManager().anythingByUser());
}

// ------------------------
/**
   @builtin Pkg::PkgAnyToDelete () -> bool

   return true if any packages are to be deleted

*/
YCPValue
PkgModuleFunctions::PkgAnyToDelete ()
{
    return YCPBoolean (_y2pm.packageManager().anythingToDelete ());
}

// ------------------------
/**
   @builtin Pkg::AnyToInstall () -> bool

   return true if any packages are to be installed

*/
YCPValue
PkgModuleFunctions::PkgAnyToInstall ()
{
    return YCPBoolean (_y2pm.packageManager().anythingToInstall ());
}

// ------------------------

/* helper functions */
static void
pgk2list (YCPList &list, const PMObjectPtr& package, bool names_only)
{
    if (names_only)
    {
	list->add (YCPString (package->name()));
    }
    else
    {
	string fullname = package->name();
	fullname += (" " + package->version());
	fullname += (" " + package->release());
	fullname += (" " + (const std::string &)(package->arch()));
	list->add (YCPString (fullname));
    }
    return;
}


/**
   @builtin Pkg::FilterPackages(bool byAuto, bool byApp, bool byUser,  bool names_only) -> list of strings

   return list of filtered packages (["pkg1", "pkg2", ...] if names_only==true,
    ["pkg1 version release arch", "pkg1 version release arch", ... if
    names_only == false]

	if one of the first 3 parameters is set to true, it returns:
	byAuto:  packages you get by dependencies,
	byApp:   packages you get by selections,
	byUser:  packages the user explicitly requested.


*/


YCPValue
PkgModuleFunctions::FilterPackages(const YCPBoolean& y_byAuto, const YCPBoolean& y_byApp, const YCPBoolean& y_byUser, const YCPBoolean& y_names_only)
{
    bool byAuto = y_byAuto->value();
    bool byApp  = y_byApp->value();
    bool byUser = y_byUser->value();
    bool names_only = y_names_only->value();

    YCPList packages;
    PMManager::PMSelectableVec::const_iterator it = _y2pm.packageManager().begin();

    while ( it != _y2pm.packageManager().end() )
    {
        PMSelectablePtr selectable = *it;

        if ( selectable->to_modify() )
        {
            if ( selectable->by_auto() && byAuto ||
                 selectable->by_appl() && byApp  ||
                 selectable->by_user() && byUser   )
            {
				pgk2list (packages, selectable->theObject(), names_only);
            }
        }

        ++it;
    }

	return packages;
}

/**
   @builtin Pkg::GetPackages(symbol which, bool names_only) -> list of strings

   return list of packages (["pkg1", "pkg2", ...] if names_only==true,
    ["pkg1 version release arch", "pkg1 version release arch", ... if
    names_only == false]

   'which' defines which packages are returned:

   `installed	all installed packages
   `selected	all selected but not yet installed packages
   `available	all available packages (from the installation source)

*/

YCPValue
PkgModuleFunctions::GetPackages(const YCPSymbol& y_which, const YCPBoolean& y_names_only)
{
    string which = y_which->symbol();
    bool names_only = y_names_only->value();

    YCPList packages;

    for (PMManager::PMSelectableVec::const_iterator it = _y2pm.packageManager().begin();
	 it != _y2pm.packageManager().end();
	 ++it)
    {
	PMObjectPtr package;
	if (which == "installed")
	{
	    package = (*it)->installedObj();
	}
	else if (which == "selected")
	{
	    if (!((*it)->to_install()))
	    {
		continue;
	    }
	    package = (*it)->candidateObj();
	}
	else if (which == "available")
	{
	    for (PMSelectable::PMObjectList::const_iterator ait = (*it)->av_begin();
		 ait != (*it)->av_end(); ++ait)
	    {
		pgk2list (packages, (*ait), names_only);
	    }
	    continue;
	}
	else
	{
	    return YCPError ("Wrong parameter for Pkg::GetPackages");
	}

	if (package)
	    pgk2list (packages, package, names_only);
    }

    return packages;
}


/**
 * @builtin PkgUpdateAll (bool delete_unmaintained) -> [ integer affected, integer unknown ]
 *
 * mark all packages for installation which are installed and have
 * an available candidate.
 *
 * @return [ integer affected, integer unknown ]
 *
 * This will mark packages for installation *and* for deletion (if a
 * package provides/obsoletes another package)
 *
 * !!! DOES NOT SOLVE !!
 */

YCPValue
PkgModuleFunctions::PkgUpdateAll (const YCPBoolean& del)
{
    PMUpdateStats stats;
    stats.delete_unmaintained = del->value();
    _y2pm.packageManager().doUpdate (stats);

    YCPList ret;
    ret->add (YCPInteger ((long long)stats.totalToInstall()));
    ret->add (YCPInteger ((long long)_y2pm.packageManager().updateSize()));
    return ret;
}


/**
   @builtin Pkg::PkgInstall (string package) -> boolean

   Select package for installation

*/
YCPValue
PkgModuleFunctions::PkgInstall (const YCPString& p)
{
    PMSelectablePtr selectable = getPackageSelectable (p->value ());

    if (!selectable)
    {
	return YCPBoolean (false);
    }
    return YCPBoolean (selectable->user_set_install());
}


/**
   @builtin Pkg::PkgSrcInstall (string package) -> boolean

   Select source of package for installation

*/
YCPValue
PkgModuleFunctions::PkgSrcInstall (const YCPString& p)
{
    PMSelectablePtr selectable = getPackageSelectable (p->value ());

    if (!selectable)
    {
	return YCPBoolean (false);
    }
    return YCPBoolean (selectable->set_source_install(true));
}


/**
   @builtin Pkg::PkgDelete (string package) -> boolean

   Select package for deletion

*/
YCPValue
PkgModuleFunctions::PkgDelete (const YCPString& p)
{
    PMSelectablePtr selectable = getPackageSelectable (p->value ());
    if (!selectable)
    {
	return YCPBoolean (false);
    }
    return YCPBoolean (selectable->user_set_delete());
}


/**
   @builtin Pkg::PkgNeutral (string package) -> boolean

   Set package to neutral (drop install/delete flags)

*/
YCPValue
PkgModuleFunctions::PkgNeutral (const YCPString& p)
{
    PMSelectablePtr selectable = getPackageSelectable (p->value ());
    if (!selectable)
    {
	return YCPBoolean (false);
    }
    selectable->setNothingSelected();
    return YCPBoolean (true);
}


/**
 * @builtin Pkg::Reset () -> boolean
 *
 * Reset most internal stuff on the package manager.
 */
YCPValue
PkgModuleFunctions::PkgReset ()
{
    _y2pm.selectionManager().setNothingSelected();
    _y2pm.packageManager().setNothingSelected();

    // FIXME also reset "conflict ignore list" in UI

    return YCPBoolean (true);
}


/**
   @builtin Pkg::PkgSolve () -> boolean
   Optional: Pkg::PkgSolve (true) to filter all conflicts with installed packages
	(installed packages will be preferred)

   Solve current package dependencies

*/
YCPValue
PkgModuleFunctions::PkgSolve (const YCPBoolean& filter)
{
    bool filter_conflicts_with_installed = false;

    if (! filter.isNull ())
    {
	filter_conflicts_with_installed = filter->value();

    }

    PkgDep::ResultList good;
    PkgDep::ErrorResultList bad;

    if (!_y2pm.packageManager().solveInstall(good, bad, filter_conflicts_with_installed))
    {
	_solve_errors = bad.size();
	y2error ("%zd packages failed:", bad.size());

	std::ofstream out ("/var/log/YaST2/badlist");
	out << bad.size() << " packages failed" << std::endl;
	for( PkgDep::ErrorResultList::const_iterator p = bad.begin();
	     p != bad.end(); ++p )
	{
	    out << *p << std::endl;
	}

	return YCPBoolean (false);
    }
    return YCPBoolean (true);
}


/**
   @builtin Pkg::PkgSolveErrors() -> integer
   only valid after a call of PkgSolve that returned false
   return number of fails
*/
YCPValue
PkgModuleFunctions::PkgSolveErrors()
{
    return YCPInteger (_solve_errors);
}

/**
   @builtin Pkg::PkgCommit (integer medianr) -> [ int successful, list failed, list remaining, list srcremaining ]

   Commit package changes (actually install/delete packages)

   the 'successful' value will be negative, if installation was aborted !

   if medianr == 0, all packages regardless of media are installed
   if medianr > 0, only packages from this media are installed

*/
YCPValue
PkgModuleFunctions::PkgCommit (const YCPInteger& media)
{
    int medianr = media->value ();

    if (medianr < 0)
    {
	return YCPError ("Bad args to Pkg::PkgCommit");
    }

    std::list<std::string> errors;
    std::list<std::string> remaining;
    std::list<std::string> srcremaining;
    int count = _y2pm.commitPackages (medianr, errors, remaining, srcremaining );

    YCPList ret;
    ret->add (YCPInteger (count));

    YCPList errlist;
    for (std::list<std::string>::const_iterator it = errors.begin(); it != errors.end(); ++it)
    {
	errlist->add (YCPString (*it));
    }
    ret->add (errlist);
    YCPList remlist;
    for (std::list<std::string>::const_iterator it = remaining.begin(); it != remaining.end(); ++it)
    {
	remlist->add (YCPString (*it));
    }
    ret->add (remlist);
    YCPList srclist;
    for (std::list<std::string>::const_iterator it = srcremaining.begin(); it != srcremaining.end(); ++it)
    {
	srclist->add (YCPString (*it));
    }
    ret->add (srclist);
    return ret;
}

/**
   @builtin Pkg::GetBackupPath () -> string

   get current path for update backup of rpm config files
*/

YCPValue
PkgModuleFunctions::GetBackupPath ()
{
    return YCPString (_y2pm.instTarget().getBackupPath().asString());
}

/**
   @builtin Pkg::SetBackupPath (string path) -> void

   set current path for update backup of rpm config files
*/
YCPValue
PkgModuleFunctions::SetBackupPath (const YCPString& p)
{
    Pathname path (p->value());
    _y2pm.instTarget().setBackupPath (path);
    return YCPVoid();
}


/**
   @builtin Pkg::CreateBackups  (boolean flag) -> void

   whether to create package backups during install or removal
*/
YCPValue
PkgModuleFunctions::CreateBackups (const YCPBoolean& flag)
{
    _y2pm.instTarget ().createPackageBackups (flag->value ());
    return YCPVoid ();
}
