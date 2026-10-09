package airx.media.ndi
{
	import flash.desktop.NativeApplication;
	import flash.display.Sprite;
	import flash.display.BitmapData;
	import flash.events.Event;
	import flash.utils.setTimeout;
	import flash.utils.setInterval;
	import air.media.MediaBuffer;
	import airx.media.ndi.NDIVideoRenderer;
	
	public class NDIVideoRendererSample extends Sprite
	{
		// We use a 'target' sprite to draw on (uses an enterFrame event listener to pull frames from NDI) via a single MediaBuffer object
		// which is set up as the render source for the sprite, and then updated with the NDI frames.
		
		private var _target : Sprite;
		private var _container : Sprite;
		private var _targetSet : Boolean;
		private var _renderer : NDIVideoRenderer;
		private var _angle : Number = 0.0;

		public function NDIVideoRendererSample()
		{
			stage.scaleMode = "noScale";
			stage.align = "topLeft";

			// use a container so that we can easily rotate it as a demo
			_container = new Sprite();
			_target = new Sprite();
			_container.addChild(_target);
			this.addChild(_container);
			_container.x =  stage.stageWidth  / 2;
			_container.y =  stage.stageHeight / 2;
			_target.x    = -stage.stageWidth  / 2;
			_target.y    = -stage.stageHeight / 2;

			// start after a short delay
			setTimeout(startNDI, 2000);
		}

		private function startNDI() : void
		{
			// create the ANE and initialize with our rendering target
			_renderer = new NDIVideoRenderer();
			var bSuccess : Boolean = _renderer.initialize();
			trace("Initialized NDI extension: success = " + bSuccess);
			if (bSuccess)
			{
				trace("Configuring target");
				bSuccess = _renderer.setTarget(_target, stage.stageWidth, stage.stageHeight);
				if (bSuccess)
				{
					_targetSet = true;
					stage.addEventListener("resize", function(e:Event):void {
						trace("Stage/target resize to " + stage.stageWidth + "x" + stage.stageHeight);
						_renderer.setTarget(_target, stage.stageWidth, stage.stageHeight);
						_container.x =  stage.stageWidth  / 2;
						_container.y =  stage.stageHeight / 2;
						_target.x    = -stage.stageWidth  / 2;
						_target.y    = -stage.stageHeight / 2;
					});
				}
			}
			if (bSuccess)
			{
				listSourcesAsync();
			}
			else
			{
				closeApp();
			}
		}

		private function listSourcesAsync() : void
		{
			// trigger the sources asynchronously, and start the first one that's received.
			_renderer.addEventListener("sources", function(e : Event) : void {
				var list : Vector.<String> = _renderer.sources;
				trace("Source list available: " + list.length + " sources");
				if (list.length)
				{
					for (var i : uint = 0; i < list.length; i++) trace("Found a source: " + list[i]);
					startVideo(list[0]);
				}
				else
				{
					closeApp();
				}
			});
			_renderer.requestSources();
		}

		private function startVideo(source : String) : void
		{
			var bSuccess : Boolean = _renderer.startCapture(source);
			if (bSuccess)
			{
				trace("STARTED VIDEO");
				addEventListener(Event.ENTER_FRAME, rotateVideo);
				setTimeout(stopVideo, 30000);
			}
			else
			{
				trace("FAILED TO START VIDEO");
				closeApp();
			}
		}
		
		private function stopVideo() : void
		{
			removeEventListener(Event.ENTER_FRAME, rotateVideo);
			_renderer.stopCapture();
			closeApp();
		}
		
		private function closeApp() : void
		{
			_renderer.dispose();
			_renderer = null;
			trace("Closed down");
			NativeApplication.nativeApplication.exit();
		}

		private function rotateVideo(e: Event) : void
		{
			_container.rotation = _angle;
			_angle += 5;
		}
	}
}
