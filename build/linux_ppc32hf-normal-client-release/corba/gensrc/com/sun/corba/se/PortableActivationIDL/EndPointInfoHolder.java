package com.sun.corba.se.PortableActivationIDL;

/**
* com/sun/corba/se/PortableActivationIDL/EndPointInfoHolder.java .
* Generated by the IDL-to-Java compiler (portable), version "3.2"
* from /var/lib/jenkins/ws/workspace/zulu8/linux/ppchf/c1/build/generic/ca/release/crossbuild/zulu8-emb-dev/corba/src/share/classes/com/sun/corba/se/PortableActivationIDL/activation.idl
* Monday, August 3, 2020 4:28:24 PM MSK
*/

public final class EndPointInfoHolder implements org.omg.CORBA.portable.Streamable
{
  public com.sun.corba.se.PortableActivationIDL.EndPointInfo value = null;

  public EndPointInfoHolder ()
  {
  }

  public EndPointInfoHolder (com.sun.corba.se.PortableActivationIDL.EndPointInfo initialValue)
  {
    value = initialValue;
  }

  public void _read (org.omg.CORBA.portable.InputStream i)
  {
    value = com.sun.corba.se.PortableActivationIDL.EndPointInfoHelper.read (i);
  }

  public void _write (org.omg.CORBA.portable.OutputStream o)
  {
    com.sun.corba.se.PortableActivationIDL.EndPointInfoHelper.write (o, value);
  }

  public org.omg.CORBA.TypeCode _type ()
  {
    return com.sun.corba.se.PortableActivationIDL.EndPointInfoHelper.type ();
  }

}
