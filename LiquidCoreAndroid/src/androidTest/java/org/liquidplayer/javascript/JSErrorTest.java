//
// JSErrorTest.java
//
// AndroidJSCore project
// https://github.com/ericwlange/AndroidJSCore/
//
// LiquidPlayer project
// https://github.com/LiquidPlayer
//
// Created by Eric Lange
//
/*
 Copyright (c) 2014-2016 Eric Lange. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 - Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 - Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
package org.liquidplayer.javascript;

import org.junit.Test;

import static org.junit.Assert.*;
import static org.hamcrest.Matchers.*;

public class JSErrorTest {

    @Test
    public void TestJSErrorAndJSException() throws Exception {
        JSContext context = new JSContext();
        TestJSErrorAndJSException(context);
    }

    public void TestJSErrorAndJSException(JSContext context) throws Exception {

        JSError error = new JSError(context, "This is an error message");
        assertThat(error.message(),is("This is an error message"));
        assertThat(error.name(),is("Error"));
        assertTrue(error.stack().contains("at Error (native)"));

        JSError error2 = new JSError(context, "Message2");
        assertThat(error2.message(),is("Message2"));
        assertThat(error2.name(),is("Error"));
        assertTrue(error2.stack().contains("at Error (native)"));

        JSError error3 = new JSError(context);
        assertThat(error3.message(),is("Error"));
        assertThat(error3.name(),is("Error"));
        assertTrue(error3.stack().contains("at Error (native)"));

        JSFunction fail = new JSFunction(context, "_fail", new String[] {},
                "var undef; var foo = undef.accessme;",
                "fail.js", 10) {
        };
        try {
            fail.call();
            assertFalse(true); // should not get here
        } catch (JSException e) {
            JSError error4 = e.getError();
            assertThat(error4.message(),is("Cannot read property 'accessme' of undefined"));
            assertThat(e.getMessage(),is("Cannot read property 'accessme' of undefined"));
            assertThat(e.toString(),is("TypeError: Cannot read property 'accessme' of undefined"));
            assertThat(error4.name(),is("TypeError"));
            assertThat(e.name(),is("TypeError"));
            assertTrue(error4.stack().contains("at _fail (fail.js:11:"));
            assertTrue(e.stack().contains("at _fail (fail.js:11:"));
        }

        try {
            context.property("_error_",error);
            context.evaluateScript("throw _error_;","main.js",1);
            assertFalse(true); // should not get here
        } catch (JSException e) {
            JSError error4 = e.getError();
            assertThat(error4.message(),is("This is an error message"));
            assertThat(e.getMessage(),is("This is an error message"));
            assertThat(e.toString(),is("Error: This is an error message"));
            assertThat(error4.name(),is("Error"));
            assertThat(e.name(),is("Error"));
            assertTrue(error4.stack().contains("at Error (native)"));
            assertTrue(e.stack().contains("at Error (native)"));
        }

        try {
            throw new JSException(error);
        } catch (JSException e) {
            JSError error5 = e.getError();
            assertThat(error5.message(),is("This is an error message"));
            assertThat(e.getMessage(),is("This is an error message"));
            assertThat(e.toString(),is("Error: This is an error message"));
            assertThat(error5.name(),is("Error"));
            assertThat(e.name(),is("Error"));
            assertTrue(error5.stack().contains("at Error (native)"));
            assertTrue(e.stack().contains("at Error (native)"));
        }

        try {
            throw new JSException(context,"Another exception");
        } catch (JSException e) {
            JSError error5 = e.getError();
            assertThat(error5.message(),is("Another exception"));
            assertThat(e.getMessage(),is("Another exception"));
            assertThat(e.toString(),is("Error: Another exception"));
            assertThat(error5.name(),is("Error"));
            assertThat(e.name(),is("Error"));
            assertTrue(error5.stack().contains("at Error (native)"));
            assertTrue(e.stack().contains("at Error (native)"));
        }

        try {
            throw new JSException(context,null);
        } catch (JSException e) {
            assertNotNull(e.getError());
            assertThat(e.getMessage(),is("Error"));
            assertThat(e.toString(),is("Error: Error"));
            assertThat(e.name(),is("Error"));
            assertTrue(e.stack().contains("at Error (native)"));
        }
    }

    @org.junit.After
    public void shutDown() {
        Runtime.getRuntime().gc();
    }
}