/**
 * Sample module-info.java demonstrating all JPMS constructs
 */
@Deprecated
module com.example.full.module {

    /*
     * ---- REQUIRES ----
     */

    // Simple requires
    requires java.base;

    // Requires with modifiers
    requires transitive java.sql;
    requires static java.logging;

    /*
     * ---- EXPORTS ----
     */

    // Unqualified export
    exports com.example.api;

    // Qualified export
    exports com.example.internal
        to com.example.friend,
           com.example.another.friend;

    /*
     * ---- OPENS ----
     */

    // Unqualified open
    opens com.example.model;

    // Qualified open
    opens com.example.secret
        to com.fasterxml.jackson.databind,
           com.google.gson;

    /*
     * ---- SERVICES ----
     */

    // Service usage
    uses com.example.spi.Plugin;

    // Service provision
    provides com.example.spi.Plugin
        with com.example.impl.PluginImpl,
             com.example.impl.AnotherPluginImpl;
}
