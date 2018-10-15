public class HelloTense {
	static {
		System.loadLibrary("tense");
		System.loadLibrary("tense_java");
	}

	private native boolean initialise();
	private native boolean destroy();
	private native void health_check();

	public static void main(String[] args) {
		HelloTense tense = new HelloTense();
		if (tense.initialise()) {
			tense.health_check();
		} else {
			System.out.println("Tense intialisation failed");
		}
		tense.destroy();
	}
}