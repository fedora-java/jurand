import A.B.C;
import A.B.E;
import A.B.F;

interface Array {
	void a(@C Object ... objects);
	void b(@C Object [] objects);
	@C Object [] c(@C Object ... objects);
	@C Object [] d(@E Object @F[] objects);
	@C Object @C[] e(@C Object @C[] objects);
}
