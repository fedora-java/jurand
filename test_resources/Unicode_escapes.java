import com.example.TestAnnotation;
import com.example\u002FSubPackage\u002ESecondAnnotation;

\u0040TestAnnotation
\u0040com.example\u002FSubPackage\u002ESecondAnnotation(value = "test")
class Unicode_escapes {
    // This file tests Unicode escape sequences:
    // \u0040 = @ (at symbol)
    // \u002F = / (forward slash)
    // \u002E = . (period/dot)
    // \u002A = * (asterisk)
    
    public void testMethod() {
        // Some comment with unicode escape \u0040
        String test = "Unicode \u0040 in string";
    }
}
