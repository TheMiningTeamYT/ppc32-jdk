// This file is an automatically generated file, please do not edit this file, modify the WrapperGenerator.java file instead !

package sun.awt.X11;

import sun.misc.*;

import sun.util.logging.PlatformLogger;
public class XIMPreeditStateNotifyCallbackStruct extends XWrapperBase { 
	private Unsafe unsafe = XlibWrapper.unsafe; 
	private final boolean should_free_memory;
	public static int getSize() { return 4; }
	public int getDataSize() { return getSize(); }

	long pData;

	public long getPData() { return pData; }


	public XIMPreeditStateNotifyCallbackStruct(long addr) {
		log.finest("Creating");
		pData=addr;
		should_free_memory = false;
	}


	public XIMPreeditStateNotifyCallbackStruct() {
		log.finest("Creating");
		pData = unsafe.allocateMemory(getSize());
		should_free_memory = true;
	}


	public void dispose() {
		log.finest("Disposing");
		if (should_free_memory) {
			log.finest("freeing memory");
			unsafe.freeMemory(pData); 
	}
		}
	public long get_state() { log.finest("");return (Native.getLong(pData+0)); }
	public void set_state(long v) { log.finest(""); Native.putLong(pData+0, v); }


	String getName() {
		return "XIMPreeditStateNotifyCallbackStruct"; 
	}


	String getFieldsAsString() {
		StringBuilder ret = new StringBuilder(40);

		ret.append("state = ").append( get_state() ).append(", ");
		return ret.toString();
	}


}



