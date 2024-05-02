package {

import com.harman.extension.PlayAssetStatus;
import com.harman.extension.PlayAssetDelivery;
import com.harman.extension.PlayAssetDeliveryEvent;
import com.harman.extension.AssetFile;

import flash.display.Sprite;
import flash.filesystem.File;
import flash.filesystem.FileMode;
import flash.filesystem.FileStream;
import flash.system.Capabilities;
import flash.system.System;
import flash.text.TextField;
import flash.events.Event;
import flash.events.MouseEvent;


public class TestPlayAssetDelivery extends Sprite {
    private var _ane : PlayAssetDelivery;
    private var _tf : TextField;
    private var _size : uint;
    private var _sInstallTime : Sprite;
    private var _sFastFollow : Sprite;
    private var _sOnDemand: Sprite;

    private function Trace(str : String) : void
    {
        trace(str);
        _tf.appendText(str);
    }
    
    public function TestPlayAssetDelivery() {
        _tf = new TextField();
        _tf.text = "Starting up\n";
        _tf.width = stage.stageWidth;
        _tf.height = stage.stageHeight;
        addChild(_tf);
        var success : Boolean = false;

        if (PlayAssetDelivery.isSupported)
        {
            Trace("ANE is supported\n");
            _ane = new PlayAssetDelivery();
            _ane.addEventListener(PlayAssetDeliveryEvent.PLAY_ASSET_UPDATE, onStatus);
            success = _ane.initAssetDelivery();
            _ane.debugMode = true;
        }
        if (success)
        {
            // add buttons for the three types of asset pack
            _size = stage.stageWidth / 3;
            _sInstallTime = createButton(0, 0x808080);
            _sFastFollow = createButton(1, 0x808080);
            _sOnDemand = createButton(2, 0x808080);
        }
        else
        {
            Trace("FAILED TO INITIALIZE\n");
        }
    }
    
    private function drawSprite(s : Sprite, colr : uint) : void
    {
        s.graphics.beginFill(0);
        s.graphics.drawRect(0, 0, _size, _size);
        s.graphics.endFill();
        s.graphics.beginFill(colr);
        s.graphics.drawRect(1, 1, _size-2, _size-2);
        s.graphics.endFill();
    }
    
    private function createButton(idx : uint, colr : uint) : Sprite
    {
        // create a sprite at the bottom of the screen (location based on this index)
        var s : Sprite = new Sprite();
        drawSprite(s, colr);
        s.x = _size * idx;
        s.y = stage.stageHeight - _size;
        s.addEventListener(MouseEvent.CLICK, onClicked);
        addChild(s);
        return s;
    }
    
    private function onClicked(e : MouseEvent) : void
    {
        // which asset pack are we after?
        var strAssetPack : String;
        var isInstallTime : Boolean = false;
        var s : Sprite = e.target as Sprite;
        if (s == _sInstallTime) { strAssetPack = "install_time_asset_pack"; isInstallTime = true; }
        else if (s == _sFastFollow) strAssetPack = "fast_follow_asset_pack";
        else if (s == _sOnDemand) strAssetPack = "on_demand_asset_pack";
        
        // different APIs..!
        if (isInstallTime)
        {
            Trace("Checking status for install-time asset pack\n");
            var af : AssetFile = _ane.openInstallTimeAsset("install_time_asset_pack_test_file.txt");
            if (af && af.valid)
            {
                Trace("Asset contents: " + af.readUTFBytes(af.bytesAvailable) + "\n");
                af.close();
                drawSprite(s, 0x00FF00);
            }
            else
            {
                Trace("Could not read install-time asset file\n");
                drawSprite(s, 0xFF0000);
            }
        }
        else if (strAssetPack)
        {
            Trace("Checking status for asset pack: " + strAssetPack + "\n");
            if (!checkAssetPack(strAssetPack, s))
            {
                Trace("Asset pack not found: requesting\n");
                drawSprite(s, 0x000088);
                _ane.fetchAssetPack(strAssetPack);
            }
        }
        else
        {
            Trace("NO ASSET PACK SELECTION\n");
            drawSprite(s, 0x202020);
        }
    }

    private function checkAssetPack(strAssetPack : String, s : Sprite) : Boolean
    {
        // first we get the location to see if it's there
        var strLocation : String = _ane.getAssetPackLocation(strAssetPack);
        if (strLocation)
        {
            drawSprite(s, 0x008000);
            Trace("Asset pack location: " + strLocation + "\n");
            // now try to access a file...
            var strPath : String = _ane.getAssetAbsolutePath(strAssetPack, strAssetPack + "_test_file.txt");
            Trace("Absolute asset path = " + strPath + "\n");
            if (!strPath)
            {
                strPath = strLocation + "/" + strAssetPack + "_test_file.txt";
                Trace("Manual path = " + strPath + "\n");
            }
            var f : File = new File(strPath);
            if (f.exists)
            {
                Trace("File exists\n");
                var fs : FileStream = new FileStream();
                fs.open(f, FileMode.READ);
                Trace("File contents: " + fs.readUTFBytes(fs.bytesAvailable) + "\n");
                fs.close();
                drawSprite(s, 0x00FF00);
            }
            else
            {
                Trace("File does not exist\n");
                drawSprite(s, 0x808000);
            }
            return true;
        }
        return false;
    }
    
    private function onStatus(e : PlayAssetDeliveryEvent) : void
    {
        Trace("Status event -> " + e.assetPackName + ", " + e.status + "\n");
        var s : Sprite = null;
        switch (e.assetPackName)
        {
        case "install_time_asset_pack":
            s = _sInstallTime;
            break;
        case "fast_follow_asset_pack":
            s = _sFastFollow;
            break;
        case "on_demand_asset_pack":
            s = _sOnDemand;
            break;
        }
        if (s)
        {
            // check the progress
            switch (e.status)
            {
            case PlayAssetStatus.COMPLETED:
                checkAssetPack(e.assetPackName, s);
                break;
            case PlayAssetStatus.FAILED:
                drawSprite(s, 0xFF0000);
                break;
            case PlayAssetStatus.DOWNLOADING:
                var total : uint = _ane.getTotalBytesToDownLoad(e.assetPackName);
                var sofar : uint = _ane.getByteDownloaded(e.assetPackName);
                Trace("Download Progress = " + (uint)((sofar * 100) / total) + "%\n");
                break;
            case PlayAssetStatus.TRANSFERRING:
                Trace("Transfer Progress = " + _ane.getTransferProgressPercentage(e.assetPackName) + "%\n");
                break;
            }
        }
    }

}
}
