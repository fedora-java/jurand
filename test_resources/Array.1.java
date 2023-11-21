import A.B.D;
import A.B.E;
import A.B.F;

interface Array {
	void a(Object @D ... objects);
	void b(Object @D[] objects);
	Object @D[] c(Object @D ... objects);
	Object @D[] d(@E Object @F[] objects);
	Object [] e(Object [] objects);
}
