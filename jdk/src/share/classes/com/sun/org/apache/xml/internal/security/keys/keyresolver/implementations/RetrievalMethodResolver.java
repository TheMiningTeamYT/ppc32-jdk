/*
 * reserved comment block
 * DO NOT REMOVE OR ALTER!
 */
/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
package com.sun.org.apache.xml.internal.security.keys.keyresolver.implementations;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.security.PublicKey;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;
import java.util.Set;

import javax.xml.parsers.ParserConfigurationException;

import com.sun.org.apache.xml.internal.security.c14n.CanonicalizationException;
import com.sun.org.apache.xml.internal.security.exceptions.XMLSecurityException;
import com.sun.org.apache.xml.internal.security.keys.content.RetrievalMethod;
import com.sun.org.apache.xml.internal.security.keys.content.x509.XMLX509Certificate;
import com.sun.org.apache.xml.internal.security.keys.keyresolver.KeyResolver;
import com.sun.org.apache.xml.internal.security.keys.keyresolver.KeyResolverException;
import com.sun.org.apache.xml.internal.security.keys.keyresolver.KeyResolverSpi;
import com.sun.org.apache.xml.internal.security.keys.storage.StorageResolver;
import com.sun.org.apache.xml.internal.security.signature.XMLSignatureInput;
import com.sun.org.apache.xml.internal.security.transforms.Transforms;
import com.sun.org.apache.xml.internal.security.utils.Constants;
import com.sun.org.apache.xml.internal.security.utils.XMLUtils;
import com.sun.org.apache.xml.internal.security.utils.resolver.ResourceResolver;
import org.w3c.dom.Attr;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.xml.sax.SAXException;

/**
 * The RetrievalMethodResolver can retrieve public keys and certificates from
 * other locations. The location is specified using the ds:RetrievalMethod
 * element which points to the location. This includes the handling of raw
 * (binary) X.509 certificate which are not encapsulated in an XML structure.
 * If the retrieval process encounters an element which the
 * RetrievalMethodResolver cannot handle itself, resolving of the extracted
 * element is delegated back to the KeyResolver mechanism.
 *
 */
public class RetrievalMethodResolver extends KeyResolverSpi {

    private static final java.util.logging.Logger LOG =
        java.util.logging.Logger.getLogger(RetrievalMethodResolver.class.getName());

    /**
     * Method engineResolvePublicKey
     * {@inheritDoc}
     * @param element
     * @param baseURI
     * @param storage
     */
    public PublicKey engineLookupAndResolvePublicKey(
           Element element, String baseURI, StorageResolver storage
    ) {
        if (!XMLUtils.elementIsInSignatureSpace(element, Constants._TAG_RETRIEVALMETHOD)) {
            return null;
        }

        try {
            // Create a retrieval method over the given element
            RetrievalMethod rm = new RetrievalMethod(element, baseURI);
            String type = rm.getType();
            XMLSignatureInput resource = resolveInput(rm, baseURI, secureValidation);
            if (RetrievalMethod.TYPE_RAWX509.equals(type)) {
                // a raw certificate, direct parsing is done!
                X509Certificate cert = getRawCertificate(resource);
                if (cert != null) {
                    return cert.getPublicKey();
                }
                return null;
             }
             Element e = obtainReferenceElement(resource, secureValidation);

             // Check to make sure that the reference is not to another RetrievalMethod
             // which points to this element
             if (XMLUtils.elementIsInSignatureSpace(e, Constants._TAG_RETRIEVALMETHOD)) {
                 if (secureValidation) {
                     if (LOG.isLoggable(java.util.logging.Level.FINE)) {
                         String error = "Error: It is forbidden to have one RetrievalMethod "
                             + "point to another with secure validation";
                         LOG.log(java.util.logging.Level.FINE, error);
                     }
                     return null;
                 }
                 RetrievalMethod rm2 = new RetrievalMethod(e, baseURI);
                 XMLSignatureInput resource2 = resolveInput(rm2, baseURI, secureValidation);
                 Element e2 = obtainReferenceElement(resource2, secureValidation);
                 if (e2 == element) {
                     LOG.log(java.util.logging.Level.FINE, "Error: Can't have RetrievalMethods pointing to each other");
                     return null;
                 }
             }

             return resolveKey(e, baseURI, storage);
         } catch (XMLSecurityException ex) {
             LOG.log(java.util.logging.Level.FINE, "XMLSecurityException", ex);
         } catch (CertificateException ex) {
             LOG.log(java.util.logging.Level.FINE, "CertificateException", ex);
         } catch (IOException ex) {
             LOG.log(java.util.logging.Level.FINE, "IOException", ex);
         } catch (ParserConfigurationException e) {
             LOG.log(java.util.logging.Level.FINE, "ParserConfigurationException", e);
         } catch (SAXException e) {
             LOG.log(java.util.logging.Level.FINE, "SAXException", e);
         }
         return null;
    }

    /**
     * Method engineResolveX509Certificate
     * {@inheritDoc}
     * @param element
     * @param baseURI
     * @param storage
     */
    public X509Certificate engineLookupResolveX509Certificate(
        Element element, String baseURI, StorageResolver storage) {
        if (!XMLUtils.elementIsInSignatureSpace(element, Constants._TAG_RETRIEVALMETHOD)) {
             return null;
        }

        try {
            RetrievalMethod rm = new RetrievalMethod(element, baseURI);
            String type = rm.getType();
            XMLSignatureInput resource = resolveInput(rm, baseURI, secureValidation);
            if (RetrievalMethod.TYPE_RAWX509.equals(type)) {
                return getRawCertificate(resource);
            }

            Element e = obtainReferenceElement(resource, secureValidation);

            // Check to make sure that the reference is not to another RetrievalMethod
            // which points to this element
            if (XMLUtils.elementIsInSignatureSpace(e, Constants._TAG_RETRIEVALMETHOD)) {
                if (secureValidation) {
                    if (LOG.isLoggable(java.util.logging.Level.FINE)) {
                        String error = "Error: It is forbidden to have one RetrievalMethod "
                            + "point to another with secure validation";
                        LOG.log(java.util.logging.Level.FINE, error);
                    }
                    return null;
                }
                RetrievalMethod rm2 = new RetrievalMethod(e, baseURI);
                XMLSignatureInput resource2 = resolveInput(rm2, baseURI, secureValidation);
                Element e2 = obtainReferenceElement(resource2, secureValidation);
                if (e2 == element) {
                    LOG.log(java.util.logging.Level.FINE, "Error: Can't have RetrievalMethods pointing to each other");
                    return null;
                }
            }

            return resolveCertificate(e, baseURI, storage);
        } catch (XMLSecurityException ex) {
            LOG.log(java.util.logging.Level.FINE, "XMLSecurityException", ex);
        } catch (CertificateException ex) {
            LOG.log(java.util.logging.Level.FINE, "CertificateException", ex);
        } catch (IOException ex) {
            LOG.log(java.util.logging.Level.FINE, "IOException", ex);
        } catch (ParserConfigurationException e) {
            LOG.log(java.util.logging.Level.FINE, "ParserConfigurationException", e);
        } catch (SAXException e) {
            LOG.log(java.util.logging.Level.FINE, "SAXException", e);
        }
        return null;
    }

    /**
     * Retrieves a x509Certificate from the given information
     * @param e
     * @param baseURI
     * @param storage
     * @return a x509Certificate from the given information
     * @throws KeyResolverException
     */
    private static X509Certificate resolveCertificate(
        Element e, String baseURI, StorageResolver storage
    ) throws KeyResolverException {
        if (LOG.isLoggable(java.util.logging.Level.FINE)) {
            LOG.log(java.util.logging.Level.FINE, "Now we have a {" + e.getNamespaceURI() + "}"
                + e.getLocalName() + " Element");
        }
        // An element has been provided
        if (e != null) {
            return KeyResolver.getX509Certificate(e, baseURI, storage);
        }
        return null;
    }

    /**
     * Retrieves a PublicKey from the given information
     * @param e
     * @param baseURI
     * @param storage
     * @return a PublicKey from the given information
     * @throws KeyResolverException
     */
    private static PublicKey resolveKey(
        Element e, String baseURI, StorageResolver storage
    ) throws KeyResolverException {
        if (LOG.isLoggable(java.util.logging.Level.FINE)) {
            LOG.log(java.util.logging.Level.FINE, "Now we have a {" + e.getNamespaceURI() + "}"
                + e.getLocalName() + " Element");
        }
        // An element has been provided
        if (e != null) {
            return KeyResolver.getPublicKey(e, baseURI, storage);
        }
        return null;
    }

    private static Element obtainReferenceElement(XMLSignatureInput resource, boolean secureValidation)
        throws CanonicalizationException, ParserConfigurationException,
        IOException, SAXException, KeyResolverException {
        Element e;
        if (resource.isElement()){
            e = (Element) resource.getSubNode();
        } else if (resource.isNodeSet()) {
            // Retrieved resource is a nodeSet
            e = getDocumentElement(resource.getNodeSet());
        } else {
            // Retrieved resource is an inputStream
            byte inputBytes[] = resource.getBytes();
            e = getDocFromBytes(inputBytes, secureValidation);
            // otherwise, we parse the resource, create an Element and delegate
            LOG.log(java.util.logging.Level.FINE, "we have to parse " +  inputBytes.length + " bytes");
        }
        return e;
    }

    private static X509Certificate getRawCertificate(XMLSignatureInput resource)
        throws CanonicalizationException, IOException, CertificateException {
        byte inputBytes[] = resource.getBytes();
        // if the resource stores a raw certificate, we have to handle it
        CertificateFactory certFact =
            CertificateFactory.getInstance(XMLX509Certificate.JCA_CERT_ID);
        try (InputStream is = new ByteArrayInputStream(inputBytes)) {
            return (X509Certificate) certFact.generateCertificate(is);
        }
    }

    /**
     * Resolves the input from the given retrieval method
     * @return the input from the given retrieval method
     * @throws XMLSecurityException
     */
    private static XMLSignatureInput resolveInput(
        RetrievalMethod rm, String baseURI, boolean secureValidation
    ) throws XMLSecurityException {
        Attr uri = rm.getURIAttr();
        // Apply the transforms
        Transforms transforms = rm.getTransforms();
        ResourceResolver resRes = ResourceResolver.getInstance(uri, baseURI, secureValidation);
        XMLSignatureInput resource = resRes.resolve(uri, baseURI, secureValidation);
        if (transforms != null) {
            LOG.log(java.util.logging.Level.FINE, "We have Transforms");
            resource = transforms.performTransforms(resource);
        }
        return resource;
    }

    /**
     * Method engineResolveSecretKey
     * {@inheritDoc}
     * @param element
     * @param baseURI
     * @param storage
     */
    public javax.crypto.SecretKey engineLookupAndResolveSecretKey(
        Element element, String baseURI, StorageResolver storage
    ) {
        return null;
    }

    private static Element getDocumentElement(Set<Node> set) {
        Iterator<Node> it = set.iterator();
        Element e = null;
        while (it.hasNext()) {
            Node currentNode = it.next();
            if (currentNode != null && Node.ELEMENT_NODE == currentNode.getNodeType()) {
                e = (Element) currentNode;
                break;
            }
        }
        List<Node> parents = new ArrayList<>();

        // Obtain all the parents of the elemnt
        while (e != null) {
            parents.add(e);
            Node n = e.getParentNode();
            if (n == null || Node.ELEMENT_NODE != n.getNodeType()) {
                break;
            }
            e = (Element) n;
        }
        // Visit them in reverse order.
        ListIterator<Node> it2 = parents.listIterator(parents.size()-1);
        Element ele = null;
        while (it2.hasPrevious()) {
            ele = (Element) it2.previous();
            if (set.contains(ele)) {
                return ele;
            }
        }
        return null;
    }
}
