#include "SharedProperties.h"


using namespace juce;


SharedProperties::SharedProperties ()
    :
    PropertiesFile (
                    GetOptions () )
  , factory (
             GetOptions () ) { InitializeProperties (); }

auto
    SharedProperties::GetPatches () const -> std::unique_ptr < XmlElement >
{
    return getXmlValue (
                        "patches" );
}

void
    SharedProperties::InitializeProperties ()
{
    // Make sure the bank has every category element. Save As adds patches to
    // these, so a bank without them (e.g. a fresh install) used to crash.
    // Existing patches are kept; missing categories are added and written back.
    auto patches_element = GetPatches ();
    auto changed         = false;
    if ( patches_element == nullptr || !patches_element -> hasTagName ( "patches" ) )
    {
        patches_element = std::make_unique < XmlElement > (
                                                           "patches" );
        changed = true;
    }
    for ( const auto* name : { "basses", "drums", "leads", "keys", "pads", "sfx", "misc" } )
    {
        if ( patches_element -> getChildByName ( name ) == nullptr )
        {
            patches_element -> addChildElement (
                                                new XmlElement (
                                                                name ) );
            changed = true;
        }
    }
    if ( changed )
    {
        setValue (
                  "patches"
                , patches_element . get () );
        saveIfNeeded ();
    }
}

void
    SharedProperties::MergePatches ()
{
    auto left = factory . getXmlValue (
                                       "patches" );
    auto right = getXmlValue (
                              " patches" );

    merged = std::make_shared < XmlElement > (
                                              "patches" );

    merged -> addChildElement (
                               new XmlElement (
                                               "basses" ) );
    merged -> addChildElement (
                               new XmlElement (
                                               "drums" ) );
    merged -> addChildElement (
                               new XmlElement (
                                               "leads" ) );
    merged -> addChildElement (
                               new XmlElement (
                                               "keys" ) );
    merged -> addChildElement (
                               new XmlElement (
                                               "pads" ) );
    merged -> addChildElement (
                               new XmlElement (
                                               "sfx" ) );
    merged -> addChildElement (
                               new XmlElement (
                                               "misc" ) );
    for ( auto i = 0 ; i < merged -> getNumChildElements () ; i++ )
    {
        const auto& category = merged -> getChildElement (
                                                          i ) -> getTagName ();
        const auto& leftCategory = left -> getChildByName (
                                                           category );
        for ( auto l = 0 ; l < leftCategory -> getNumChildElements () ; l++ )
        {
            const auto& patch = leftCategory -> getChildElement (
                                                                 l );
            // Thinking about it, this is a terrible way to do this because the
            // files are independently updateable, so merging them statically
            // will just make patches stop saving.
            //
            // Abandoning the effort for the time being, as this requires more thought to approach.
        }
    }
}

auto
    SharedProperties::GetOptions () -> Options
{
    Options props;
    props . applicationName  = "SIDStep 2";
    props . filenameSuffix   = ".bank";
    props . commonToAllUsers = false;
    return props;
}

auto
    SharedProperties::GetFactoryOptions () -> Options
{
    Options props;
    props . applicationName  = "SIDStep 2";
    props . filenameSuffix   = ".factory";
    props . commonToAllUsers = false;
    return props;
}
