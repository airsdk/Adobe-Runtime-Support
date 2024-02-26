package com.harman.air
{
	import flash.display.Sprite;
	import flash.events.Event;
	import flash.utils.setTimeout;
	import flash.utils.setInterval;
	import air.media.MediaBuffer;
	import com.harman.extension.MediaBufferRender;
	
	public class MediaBufferRender extends Sprite
	{
		private var _angle : Number = 0.0;
		private var _s1 : Sprite;
		private var _s2 : Sprite;
		private var _s3 : Sprite;
		private var _buffer : MediaBuffer;
		
		[Embed(source="../../../harman-logo.png")]
		private var HarmanLogo : Class;
		
		public function MediaBufferRender()
		{
			// set up three sprites and have them rotate
			_s1 = createSprite(100, 100, 0xff0000);
			_s2 = createSprite(200, 200, 0x00ff00);
			_s3 = createSprite(300, 300, 0x0000ff);
			//setInterval(rotateSprites, 1000);
			addEventListener(Event.ENTER_FRAME, rotateSprites);
			
			// create a media buffer object
			_buffer = MediaBuffer.createFromResource(HarmanLogo);
			setTimeout(setupMediaBuffer, 5000);
		}
		
		private function setupMediaBuffer() : void
		{
			// create the ANE and set up the media buffer as the target for sprite 2
			var bSuccess : Boolean = com.harman.extension.MediaBufferRender.setupMediaBufferRendering(_buffer, _s2);
			trace("Configured media buffer for sprite, success = " + bSuccess);
			setTimeout(startChangingMediaBuffer, 5000);
		}

		private function startChangingMediaBuffer() : void
		{
			// create the ANE and set up the media buffer as the target for sprite 2
			var bSuccess : Boolean = com.harman.extension.MediaBufferRender.startChangingMediaBuffer(_buffer);
			trace("Started thread to alter media buffer, success = " + bSuccess);
		}
		
		private function createSprite(aX : uint, aY : uint, colr : uint) : Sprite
		{
			var s : Sprite = new Sprite();
			s.graphics.beginFill(colr);
			s.graphics.drawRect(0, 0, 200, 200);
			s.graphics.endFill();
			s.x = aX;
			s.y = aY;
			addChild(s);
			return s;
		}
		
		private function rotateSprites(e: Event = null) : void
		{
			trace("Rotating to " + _angle);
			_s1.rotation = _s2.rotation = _s3.rotation = _angle;
			_angle += 5;
		}
	}
}