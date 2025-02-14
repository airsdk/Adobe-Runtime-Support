package
{
	import com.harman.air.AIRDiagnostics;
	import com.harman.air.DiagnosticEvent;
	import com.harman.air.DiagnosticInfo;
	import com.harman.air.DiagnosticReport;
	import flash.desktop.NativeApplication;
	import flash.events.Event;
	import flash.display.Sprite;
	import flash.display.StageAlign;
	import flash.display.StageScaleMode;
	import flash.events.TimerEvent;
	import flash.filesystem.File;
	import flash.system.Capabilities;
	import flash.system.MessageChannel;
	import flash.system.Worker;
	import flash.system.WorkerDomain;
	import flash.ui.Multitouch;
	import flash.ui.MultitouchInputMode;
	import flash.utils.Timer;
	import flash.utils.getTimer;
	
	/**
	 * @author Andrew Frost
	 *
	 * Note: this uses both Workers and ANEs. The background Worker is using the same SWF as the primordial Worker,
	 * and references the AIRDiagnostics ANE classes - which means that the worker will also need to load in the
	 * ANE's library.swf file, for which it will need to be in the application security domain.
	 * So to avoid the worker automatically terminated on start-up, you need to pass 'true' into the createWorker
	 * method for 'giveAppPrivileges'.
	 *
	 * The primordial worker is the one that is dealing with the diagnostics (in this demo) and is configuring this
	 * just for Worker information as well as errors and long function calls. If there are any ANEs or other crashes then
	 * when the app starts up, it should find the log files left over (checkForOldDiagnostics method). In a normal
	 * shutdown, those files should be cleaned up on exit. On Android this would mean the NativeApplication.exit() call,
	 * rather than the user terminating the application via the operating system.
	 *
	 * To trigger the error, the background worker just sends a message every 5s, and the foreground worker will
	 * close the message channel after 22 seconds. The error will be reported back to the app via the diagnostics
	 * error mechanism. If the error is a 'channel closed' error, the Worker log will be read and traced out.
	 *
	 * Nothing appears on the screen for this test, the functionality needs to be checked via the trace log e.g. via
	 * ADB (or "adt -deviceLog"), or via Adobe Scout or a Flash Debugger connection.
	 */
	public class Main extends Sprite 
	{
		private var _background : Worker;
		private var _messageChannel : MessageChannel;
		private static const MESSAGE_CHANNEL_BACKGROUND_TO_PRIMORDIAL : String = "messagesToPrimordial";
		private var _diagInfo : DiagnosticInfo;
		
		public function Main() 
		{
			stage.scaleMode = StageScaleMode.NO_SCALE;
			stage.align = StageAlign.TOP_LEFT;
			stage.addEventListener(Event.DEACTIVATE, deactivate);

			// touch or gesture?
			Multitouch.inputMode = MultitouchInputMode.TOUCH_POINT;
			
			// Entry point
			
			// Set up the worker
			if (Worker.current.isPrimordial)
			{
				// set up diagnostics
				_diagInfo = DiagnosticInfo.instance;
				_diagInfo.debugFunction = debugFunc;
				// check for old reports
				checkForOldDiagnostics();
				// set up listeners for diagnostic info
				setupDiagnostics();
				_diagInfo.start( AIRDiagnostics.DiagCheckLongFunc | AIRDiagnostics.DiagLogErrorCallstacks | AIRDiagnostics.DiagLogWorker );

				// create the new worker and message channel
				_background = WorkerDomain.current.createWorker(this.loaderInfo.bytes, true); // must give app privileges to reference ANE classes
				_background.addEventListener(Event.WORKER_STATE, onBackgroundWorkerStateChange);
				_messageChannel = _background.createMessageChannel(Worker.current);
				_messageChannel.addEventListener(Event.CHANNEL_MESSAGE, onMessage);
				_background.setSharedProperty(MESSAGE_CHANNEL_BACKGROUND_TO_PRIMORDIAL, _messageChannel);
				_background.start();
				
				trace("Started up");
				
				// add a timer to close the message channel after 22s
				var tClose : Timer = new Timer(22000, 1);
				tClose.addEventListener(TimerEvent.TIMER, function(e:Event) : void {
					trace("Closing message channel");
					_messageChannel.close();
				});
				tClose.start();
				
			}
			else
			{
				_messageChannel = Worker.current.getSharedProperty(MESSAGE_CHANNEL_BACKGROUND_TO_PRIMORDIAL);
				// Background: just periodically send messages
				var tSend : Timer = new Timer(5000);
				tSend.addEventListener(TimerEvent.TIMER, function(e:Event) : void {
					trace("Sending message..");
					_messageChannel.send("Time = " + getTimer());
				});
				tSend.start();
			}
		}

		private function debugFunc(str : String) : void
		{
			trace("Debug message from ANE: " + str);
		}

		private function onBackgroundWorkerStateChange(e : Event) : void
		{
			var background : Worker = e.target as Worker;
			trace("Background worker state = " + background.state);
		}
		
		private function onMessage(e : Event) : void
		{
			trace("Message received : " + _messageChannel.receive() as String);
		}
		
		private function deactivate(e:Event):void 
		{
			// make sure the app exits when in background on Android
			// (graceful shutdown is required to clean up the diagnostic log area)
			if (Capabilities.version.startsWith("AND"))
				NativeApplication.nativeApplication.exit();
		}

		private function checkForOldDiagnostics() : void
		{
			// check if we have any old reports
			var reports : Vector.<DiagnosticReport> = _diagInfo.reports;
			if (reports.length)
			{
				// deal with them
				trace("We have " + reports.length + " diagnostic reports");
				for each (var report : DiagnosticReport in reports)
				{
					trace(report.toString());
				}
			}
			_diagInfo.clearReports();
		}
		
		private function setupDiagnostics() : void
		{
			// listen out for concerns - long-running functions, and errors
			_diagInfo.addEventListener(DiagnosticEvent.LONG_RUNNING_FUNCTION, function(e:DiagnosticEvent) : void {
				trace("DIAGNOSTIC LONG-RUNNING FUNCTION: " + e.description + " = " + e.metric);
			});
			_diagInfo.addEventListener(DiagnosticEvent.ERROR_DETAILS, function(e:DiagnosticEvent) : void {
				trace("DIAGNOSTIC ERROR: " + e.description + " = " + e.metric);
				if (e.description.startsWith("Throwing ChannelClosed error"))
				{
					trace(" -- checking for worker log --");
					var log : Vector.<String> = _diagInfo.getLog("worker");
					if (log) for (var i : uint = 0; i < log.length; i++) trace(log[i]);
					trace(" -- end of worker log --");
				}
			});
		}
	}
	
}
