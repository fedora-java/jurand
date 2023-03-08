// Regression test for issue #5
// See https://github.com/fedora-java/jurand/issues/5

class MyClass {
    int foo;
}

@interface MyAnnotation {
    int foo = 0;
}

class MyOtherClass {
    int foo;
}
