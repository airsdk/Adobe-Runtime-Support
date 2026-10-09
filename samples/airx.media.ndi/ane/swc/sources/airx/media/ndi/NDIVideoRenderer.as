package airx.media.ndi
{
import flash.external.ExtensionContext;
import flash.desktop.NativeApplication;
import flash.display.Sprite;
import flash.display.BitmapData;
import flash.events.Event;
import flash.events.EventDispatcher;
import flash.events.StatusEvent;
import air.media.MediaBuffer;
import air.media.MediaElement;

/** NDIVideoRenderer */
public class NDIVideoRenderer extends EventDispatcher
{
	private var _ane : ExtensionContext;
	private var _target : Sprite;
	private var _width : uint;
	private var _height : uint;
	private var _sources : Vector.<String>;
	private var _capturing : Boolean;
	private var _buffer : MediaBuffer;
	private var _rendering : Boolean;

	public function NDIVideoRenderer()
	{
		NativeApplication.nativeApplication.addEventListener(Event.EXITING, function(e:Event):void { dispose(); });
	}
	
	public function initialize() : Boolean
	{
		var success : Boolean = false;
		trace("Initializing NDIVideoRenderer extension context");
		// create the ANE and init NDI
		_ane = ExtensionContext.createExtensionContext("airx.media.ndi", "NDIVideoRenderer");
		if (_ane)
		{
			_ane.addEventListener(StatusEvent.STATUS, onStatusEvent);
			success = _ane.call("initializeNDI") as Boolean;
		}
		return success;
	}
	
	private function onStatusEvent(e : StatusEvent) : void
	{
		trace("NDIVideoRenderer: Received status event from NDI: level " + e.level + ", code " + e.code);
		switch (e.code)
		{
		case "source_list":
			dispatchEvent( new Event("sources") );
			break;
		case "video.started":
			// we can get frame details etc now...
			break;
		default:
			trace("NDIVideoRenderer: Unknown code");
		}
	}

	public function setTarget(target : Sprite, width : uint, height : uint) : Boolean
	{
		var success : Boolean = false;
		if (_ane)
		{
			// skip if we already have this set up
			if (target == _target && width == _width && height == _height)  return false;
			// create a media buffer
			var bmData : BitmapData = new BitmapData(width, height, true, 0);
			_buffer = MediaBuffer.createFromBitmapData(bmData);
			_target = target;
			trace("Setting NDIVideoRenderer target " + _target.name);
			success = _ane.call("setTarget", _target, _buffer) as Boolean;
		}
		return success;
	}
	
	// return any sources that we already have in the list.
	public function get sources() : Vector.<String>
	{
		if (_sources && _sources.length) return _sources;
		if (_sources == null) _sources = new Vector.<String>();
		// list the current sources
		if (_ane)
		{
			_ane.call("listSources", _sources);
		}
		return _sources;
	}
	
	// trigger a request for sources asynchronously, up to the timeout
	public function requestSources(timeoutMS : int = 5000) : void
	{
		if (_ane)
		{
			_ane.call("requestSources", timeoutMS);
		}
	}
	
	public function startCapture(source : String) : Boolean
	{
		var success : Boolean = false;
		// connect to the named source and start capturing data
		if (_ane)
		{
			if (_capturing) stopCapture();
			success = _capturing = _ane.call("startCapture", source) as Boolean;
			startRendering();
		}
		return success;
	}
	
	public function stopCapture() : Boolean
	{
		var success : Boolean = false;
		// disconnect and stop..
		if (_ane && _capturing)
		{
			stopRendering();
			_capturing = !(success = _ane.call("stopCapture") as Boolean);
		}
		return success;
	}
	
	public function dispose() : void
	{
		// clean up NDI and the ANE
		if (_ane)
		{
			if (_capturing) stopCapture();
			_ane.call("closeNDI");
			_ane.dispose();
			_ane = null;
		}
	}

	// Mechanism to use if we have a 'target' object which is the DisplayObject
	// that we want to use to display the video. This calls "render" every frame
	// which pulls a video frame from NDI and copies it to the display object.

	private function startRendering() : void
	{
		if (_rendering) return;
		// start if we have both a target and are capturing
		if (_target && _capturing)
		{
			_target.addEventListener(Event.ENTER_FRAME, onEnterFrame);
			_rendering = true;
		}
	}
	private function stopRendering() : void
	{
		if (!_rendering) return;
		if (_target) _target.removeEventListener(Event.ENTER_FRAME, onEnterFrame);
		_rendering = false;
	}
	private function onEnterFrame(e : Event) : void
	{
		_ane.call("render");
	}

}
}
