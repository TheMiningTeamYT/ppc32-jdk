// This file is an automatically generated file, please do not edit this file, modify the WrapperGenerator.java file instead !

package sun.awt.X11;

import sun.misc.*;

import sun.util.logging.PlatformLogger;
public class XIMStyles extends XWrapperBase { 
	private Unsafe unsafe = XlibWrapper.unsafe; 
	private final boolean should_free_memory;
	public static int getSize() { return 8; }
	public int getDataSize() { return getSize(); }

	long pData;

	public long getPData() { return pData; }


	public XIMStyles(long addr) {
		log.finest("Creating");
		pData=addr;
		should_free_memory = false;
	}


	public XIMStyles() {
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
	public short get_count_styles() { log.finest("");return (Native.getShort(pData+0)); }
	public void set_count_styles(short v) { log.finest(""); Native.putShort(pData+0, v); }
	public long get_supported_styles(int index) { log.finest(""); return Native.getLong(Native.getLong(pData+4)+index*Native.getLongSize()); }
	public long get_supported_styles() { log.finest("");return Native.getLong(pData+4); }
	public void set_supported_styles(long v) { log.finest(""); Native.putLong(pData + 4, v); }


	String getName() {
		return "XIMStyles"; 
	}


	String getFieldsAsString() {
		StringBuilder ret = new StringBuilder(80);

		ret.append("count_styles = ").append( get_count_styles() ).append(", ");
		ret.append("supported_styles = ").append( get_supported_styles() ).append(", ");
		return ret.toString();
	}


}



