/* Wallpaper Engine web host surface, from REA §12.
   QtWebEngine is not linked. This is the page-side API a host would inject. */
var bendwallProps = {};
var bendwallAudioFn = null;
var bendwallLastAudio = null;

var wallpaperPropertyListener = {
  applyUserProperties: function (props) {
    var k;
    if (!props) return;
    for (k in props) {
      if (props[k] && props[k].value !== undefined) bendwallProps[k] = props[k].value;
    }
  },
  applyGeneralProperties: function () {},
  setPaused: function () {}
};

function wallpaperRegisterAudioListener(fn) {
  bendwallAudioFn = fn;
}

function bendwallApplyProperties(props) {
  if (wallpaperPropertyListener && wallpaperPropertyListener.applyUserProperties)
    wallpaperPropertyListener.applyUserProperties(props);
}

function bendwallPushAudio(samples) {
  bendwallLastAudio = samples;
  if (bendwallAudioFn) bendwallAudioFn(samples);
}

function bendwallProp(name) {
  return bendwallProps[name];
}
