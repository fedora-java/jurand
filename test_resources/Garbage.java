import /*;*/ edu.umd.cs.findbugs.annotations/*;*/.Nullable; /*;*/ // ;

class Garbage {
    <T extends @Nullable(value = "true") Object> T method(@Nullable(value = "true") Object o) {
        new @Nullable(value = "true") Object();
    }
}
