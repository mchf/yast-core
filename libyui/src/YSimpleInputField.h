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

  File:		YSimpleInputField.h

  Author:	Stefan Hundhammer <sh@suse.de>

/-*/

#ifndef YSimpleInputField_h
#define YSimpleInputField_h

#include "YWidget.h"

class YSimpleInputFieldPrivate;


/**
 * Abstract base class for simple input fields with a label above the field and
 * a text value.
 **/
class YSimpleInputField : public YWidget
{
protected:
    /**
     * Constructor.
     **/
    YSimpleInputField( YWidget * parent, const string & label );

public:
    /**
     * Destructor.
     **/
    virtual ~YSimpleInputField();
    
    /**
     * Get the current value (the text entered by the user or set from the
     * outside) of this input field.
     *
     * Derived classes are required to implement this.
     **/
    virtual string value() = 0;

    /**
     * Set the current value (the text entered by the user or set from the
     * outside) of this input field.
     *
     * Derived classes are required to implement this.
     **/
    virtual void setValue( const string & text ) = 0;

    /**
     * Get the label (the caption above the input field).
     **/
    string label();

    /**
     * Set the label (the caption above the input field).
     *
     * Derived classes are free to reimplement this, but they should call this
     * base class method at the end of the overloaded function.
     **/
    virtual void setLabel( const string & label );

    /**
     * Set a property.
     * Reimplemented from YWidget.
     *
     * This function may throw YUIPropertyExceptions.
     *
     * This function returns 'true' if the value was successfully set and
     * 'false' if that value requires special handling (not in error cases:
     * those are covered by exceptions).
     **/
    virtual bool setProperty( const string & propertyName,
			      const YPropertyValue & val );

    /**
     * Get a property.
     * Reimplemented from YWidget.
     *
     * This method may throw YUIPropertyExceptions.
     **/
    virtual YPropertyValue getProperty( const string & propertyName );

    /**
     * Return this class's property set.
     * This also initializes the property upon the first call.
     *
     * Reimplemented from YWidget.
     **/
    virtual const YPropertySet & propertySet();

    /**
     * Get the string of this widget that holds the keyboard shortcut.
     *
     * Reimplemented from YWidget.
     **/
    virtual string shortcutString() { return label(); }

    /**
     * Set the string of this widget that holds the keyboard shortcut.
     *
     * Reimplemented from YWidget.
     **/
    virtual void setShortcutString( const string & str )
	{ setLabel( str ); }

    /**
     * The name of the widget property that will return user input.
     * Inherited from YWidget.
     **/
    const char * userInputProperty() { return YUIProperty_Value; }


protected:
    /**
     * Save the widget's user input to a macro recorder.
     *
     * This sample implementation saves value(). This can be reimplemented in
     * derived classes. Typically it does not make sense to call this base
     * class method in new implementations.
     **/
    virtual void saveUserInput( YMacroRecorder *macroRecorder );

private:
    
    ImplPtr<YSimpleInputFieldPrivate> priv;
};


#endif // YSimpleInputField_h