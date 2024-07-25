// This file is an automatically generated file, please do not edit this file, modify the WrapperGenerator.java file instead !

package sun.awt.X11;

import sun.misc.*;

import sun.util.logging.PlatformLogger;
public class XCrossingEvent extends XWrapperBase { 
	private Unsafe unsafe = XlibWrapper.unsafe; 
	private final boolean should_free_memory;
	public static int getSize() { return 68; }
	public int getDataSize() { return getSize(); }

	long pData;

	public long getPData() { return pData; }


	public XCrossingEvent(long addr) {
		log.finest("Creating");
		pData=addr;
		should_free_memory = false;
	}


	public XCrossingEvent() {
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
	public int get_type() { log.finest("");return (Native.getInt(pData+0)); }
	public void set_type(int v) { log.finest(""); Native.putInt(pData+0, v); }
	public long get_serial() { log.finest("");return (Native.getLong(pData+4)); }
	public void set_serial(long v) { log.finest(""); Native.putLong(pData+4, v); }
	public boolean get_send_event() { log.finest("");return (Native.getBool(pData+8)); }
	public void set_send_event(boolean v) { log.finest(""); Native.putBool(pData+8, v); }
	public long get_display() { log.finest("");return (Native.getLong(pData+12)); }
	public void set_display(long v) { log.finest(""); Native.putLong(pData+12, v); }
	public long get_window() { log.finest("");return (Native.getLong(pData+16)); }
	public void set_window(long v) { log.finest(""); Native.putLong(pData+16, v); }
	public long get_root() { log.finest("");return (Native.getLong(pData+20)); }
	public void set_root(long v) { log.finest(""); Native.putLong(pData+20, v); }
	public long get_subwindow() { log.finest("");return (Native.getLong(pData+24)); }
	public void set_subwindow(long v) { log.finest(""); Native.putLong(pData+24, v); }
	public long get_time() { log.finest("");return (Native.getULong(pData+28)); }
	public void set_time(long v) { log.finest(""); Native.putULong(pData+28, v); }
	public int get_x() { log.finest("");return (Native.getInt(pData+32)); }
	public void set_x(int v) { log.finest(""); Native.putInt(pData+32, v); }
	public int get_y() { log.finest("");return (Native.getInt(pData+36)); }
	public void set_y(int v) { log.finest(""); Native.putInt(pData+36, v); }
	public int get_x_root() { log.finest("");return (Native.getInt(pData+40)); }
	public void set_x_root(int v) { log.finest(""); Native.putInt(pData+40, v); }
	public int get_y_root() { log.finest("");return (Native.getInt(pData+44)); }
	public void set_y_root(int v) { log.finest(""); Native.putInt(pData+44, v); }
	public int get_mode() { log.finest("");return (Native.getInt(pData+48)); }
	public void set_mode(int v) { log.finest(""); Native.putInt(pData+48, v); }
	public int get_detail() { log.finest("");return (Native.getInt(pData+52)); }
	public void set_detail(int v) { log.finest(""); Native.putInt(pData+52, v); }
	public boolean get_same_screen() { log.finest("");return (Native.getBool(pData+56)); }
	public void set_same_screen(boolean v) { log.finest(""); Native.putBool(pData+56, v); }
	public boolean get_focus() { log.finest("");return (Native.getBool(pData+60)); }
	public void set_focus(boolean v) { log.finest(""); Native.putBool(pData+60, v); }
	public int get_state() { log.finest("");return (Native.getInt(pData+64)); }
	public void set_state(int v) { log.finest(""); Native.putInt(pData+64, v); }


	String getName() {
		return "XCrossingEvent"; 
	}


	String getFieldsAsString() {
		StringBuilder ret = new StringBuilder(680);

		ret.append("type = ").append( XlibWrapper.eventToString[get_type()] ).append(", ");
		ret.append("serial = ").append( get_serial() ).append(", ");
		ret.append("send_event = ").append( get_send_event() ).append(", ");
		ret.append("display = ").append( get_display() ).append(", ");
		ret.append("window = " ).append( getWindow(get_window()) ).append(", ");
		ret.append("root = ").append( get_root() ).append(", ");
		ret.append("subwindow = ").append( get_subwindow() ).append(", ");
		ret.append("time = ").append( get_time() ).append(", ");
		ret.append("x = ").append( get_x() ).append(", ");
		ret.append("y = ").append( get_y() ).append(", ");
		ret.append("x_root = ").append( get_x_root() ).append(", ");
		ret.append("y_root = ").append( get_y_root() ).append(", ");
		ret.append("mode = ").append( get_mode() ).append(", ");
		ret.append("detail = ").append( get_detail() ).append(", ");
		ret.append("same_screen = ").append( get_same_screen() ).append(", ");
		ret.append("focus = ").append( get_focus() ).append(", ");
		ret.append("state = ").append( get_state() ).append(", ");
		return ret.toString();
	}


}


