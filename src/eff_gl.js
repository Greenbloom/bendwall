function gl_init() { throw new Error("C lane only"); }
function gl_tex() { throw new Error("C lane only"); }
function gl_bind() { throw new Error("C lane only"); }
function gl_clear() { throw new Error("C lane only"); }
function gl_rgba() { throw new Error("C lane only"); }
function gl_prog() { throw new Error("C lane only"); }
function gl_eye() { throw new Error("C lane only"); }
function gl_f1() { throw new Error("C lane only"); }
function gl_f2() { throw new Error("C lane only"); }
function gl_quad() { throw new Error("C lane only"); }
function gl_png() { throw new Error("C lane only"); }
function gl_log() { throw new Error("C lane only"); }
io_eff(CID(gl_init), gl_init);
io_eff(CID(gl_tex), gl_tex);
io_eff(CID(gl_bind), gl_bind);
io_eff(CID(gl_clear), gl_clear);
io_eff(CID(gl_rgba), gl_rgba);
io_eff(CID(gl_prog), gl_prog);
io_eff(CID(gl_eye), gl_eye);
io_eff(CID(gl_f1), gl_f1);
io_eff(CID(gl_f2), gl_f2);
io_eff(CID(gl_quad), gl_quad);
io_eff(CID(gl_png), gl_png);
io_eff(CID(gl_log), gl_log);
