package com.harman.extension
{
import flash.external.ExtensionContext;
import flash.display.Sprite;
import air.media.MediaBuffer;

/** MediaBufferRender */
public class MediaBufferRender
{
	public static function setupMediaBufferRendering(mediaBuffer : MediaBuffer, sprite : Sprite) : Boolean
	{
		var success : Boolean = false;
		var ane : ExtensionContext = ExtensionContext.createExtensionContext("com.harman.ane.MediaBufferRender", "");
		if (ane) try
		{
			success = ane.call("setDisplayObjectSource", mediaBuffer, sprite) as Boolean;
		}
		catch(e : Error)
		{
			trace("Error calling setDisplayObjectSource -> " + e.toString());
		}
		return success;
	}
	
	public static function startChangingMediaBuffer(mediaBuffer : MediaBuffer) : Boolean
	{
		var success : Boolean = false;
		var ane : ExtensionContext = ExtensionContext.createExtensionContext("com.harman.ane.MediaBufferRender", "");
		if (ane) try
		{
			success = ane.call("startChangingMediaBuffer", mediaBuffer) as Boolean;
		}
		catch(e : Error)
		{
			trace("Error calling startChangingMediaBuffer -> " + e.toString());
		}
		return success;
	}

}
}
