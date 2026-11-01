function av_cap() { throw new Error("C lane only"); }
function av_video() { throw new Error("C lane only"); }
function av_tex() { throw new Error("C lane only"); }
io_eff(CID(av_cap), av_cap);
io_eff(CID(av_video), av_video);
io_eff(CID(av_tex), av_tex);
