function text_ink() { throw new Error("C lane only"); }
function text_tex() { throw new Error("C lane only"); }
io_eff(CID(text_ink), text_ink);
io_eff(CID(text_tex), text_tex);
