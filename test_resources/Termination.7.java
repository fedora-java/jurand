// Regression test for issue #5
// See https://github.com/fedora-java/jurand/issues/5
import java.io.Serial;

class MyClass {
    @Serial
    int foo;
}

@interface MyAnnotation {
    @Serial
    int foo = 0;
    /*
}

class MyOtherClass {
    @Serial
    int foo;
}
