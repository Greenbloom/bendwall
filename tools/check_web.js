import * as std from "std";

std.loadScript("host/webshim.js");

function fail(msg) {
  std.err.puts(msg + "\n");
  std.exit(1);
}

var seen = null;
wallpaperRegisterAudioListener(function (samples) { seen = samples; });
bendwallApplyProperties({
  color1: { value: "0 1 0" },
  speed: { value: 0.5 }
});
if (bendwallProp("color1") !== "0 1 0") fail("color");
if (bendwallProp("speed") !== 0.5) fail("speed");

var audio = [];
var i;
for (i = 0; i < 128; i++) audio.push(0);
audio[0] = 0.25;
audio[64] = 0.75;
bendwallPushAudio(audio);
if (!seen || seen.length !== 128) fail("len " + (seen ? seen.length : 0));
if (seen[0] !== 0.25) fail("left");
if (seen[63] !== 0) fail("left-end");
if (seen[64] !== 0.75) fail("right");
if (seen[127] !== 0) fail("right-end");
std.out.puts("WEB ok props=2 audio=128\n");
