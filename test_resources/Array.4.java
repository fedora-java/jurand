import A.B.C;
import A.B.D;
import A.B.E;

interface Array {
	void a(@C Object @D ... objects);
	void b(@C Object @D[] objects);
	@C Object @D[] c(@C Object @D ... objects);
	@C Object @D[] d(@E Object [] objects);
	@C Object @C[] e(@C Object @C[] objects);
}
