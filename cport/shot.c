/* shot.c: see shot.h. Per triangle (on the CPU, as mesher.shot does it): its normal, dropped if it has no area or
   faces away from the camera, its shade 0.30 + 0.70 * max(0, n.light), its corners on the screen. The card then fills
   the pixels with a depth test (nearest wins), one flat colour per triangle, no smoothing, as the Python one. */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "shot.h"

typedef HRESULT (WINAPI *PD3DCompile)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **);

static const char HLSL[] =
    "struct VI { float3 p : POSITION; float s : SHADE; float2 uv : TEXCOORD; };\n"
    "struct VO { float4 p : SV_Position; nointerpolation float s : SHADE; float2 uv : TEXCOORD; };\n"
    "Texture2D tx : register(t0); SamplerState sp : register(s0);\n"
    "VO vs(VI i) { VO o; o.p = float4(i.p, 1.0); o.s = i.s; o.uv = i.uv; return o; }\n"
    "float4 ps(VO i) : SV_Target { return float4(i.s * 0.86, i.s * 0.84, i.s * 0.80, 1.0); }\n"
    /* the colour view: the 2D drawing's colour straight above each point, lit by the same shade. Off the drawing, or on
       its white surround, the grey of the plain picture */
    "float4 pst(VO i) : SV_Target {\n"
    "  float3 c = float3(0.86, 0.84, 0.80);\n"
    "  if (i.uv.x >= 0 && i.uv.x <= 1 && i.uv.y >= 0 && i.uv.y <= 1) { float3 t = tx.Sample(sp, i.uv).rgb; if (!(t.r > 0.97 && t.g > 0.97 && t.b > 0.97)) c = t; }\n"
    "  return float4(saturate(c * (0.40 + 0.70 * i.s)), 1.0); }\n";

#define REL(x) do { if (x) { IUnknown_Release((IUnknown *)(x)); (x) = NULL; } } while (0)

static int fail(char *err, int n, const char *what, HRESULT hr) { snprintf(err, n, "%s (0x%08lx)", what, (unsigned long)hr); return 0; }

/* shot_keep(1): this thread keeps its Direct3D device, shaders and states between pictures (the Models preview draws
   hundreds on every core: making the device and compiling the shaders again was nearly all its time); shot_keep(0) lets
   them go. Only plain shots (no colour drawing) use the kept ones. */
typedef struct { int on, mode; ID3D11Device *D; ID3D11DeviceContext *X; ID3D11VertexShader *vs; ID3D11PixelShader *ps; ID3D11InputLayout *il; ID3D11DepthStencilState *dss; ID3D11RasterizerState *rs; char dev[160]; } ShotKeep;
static _Thread_local ShotKeep KP;
void shot_keep(int on) {
    if (!on) { REL(KP.rs); REL(KP.dss); REL(KP.il); REL(KP.ps); REL(KP.vs); REL(KP.X); REL(KP.D); memset(&KP, 0, sizeof KP); return; }
    KP.on = 1;
}
int shot_render(const double *V, int nv, const int *F, int nt, int w, int h, double elev, double azim, int mode,
                unsigned char *rgb, char *dev, int devlen, char *err, int errlen) {
    return shot_render_tex(V, nv, F, nt, w, h, elev, azim, mode, NULL, 0, 0, NULL, rgb, dev, devlen, err, errlen);
}
int shot_render_tex(const double *V, int nv, const int *F, int nt, int w, int h, double elev, double azim, int mode,
                    const unsigned char *tex, int tw, int th, const double *win,
                    unsigned char *rgb, char *dev, int devlen, char *err, int errlen) {
    if (tex && (tw < 1 || th < 1 || tw > 16384 || th > 16384 || !win || !(win[2] > win[0]) || !(win[3] > win[1]))) return fail(err, errlen, "colour drawing", 0);
    if (w < 8 || h < 8 || w > 16384 || h > 16384) return fail(err, errlen, "picture size", 0);
    /* 1. the triangles on the screen, exactly as mesher.shot works them out */
    double e_ = elev * M_PI / 180.0, a_ = azim * M_PI / 180.0;
    double cam[3] = { cos(e_) * cos(a_), cos(e_) * sin(a_), sin(e_) }, right[3] = { -sin(a_), cos(a_), 0.0 }, up[3];
    up[0] = cam[1] * right[2] - cam[2] * right[1]; up[1] = cam[2] * right[0] - cam[0] * right[2]; up[2] = cam[0] * right[1] - cam[1] * right[0];
    double L[3] = { -0.45, -0.35, 0.82 }, ll = sqrt(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]); L[0] /= ll; L[1] /= ll; L[2] /= ll;
    double r = 1.0; for (int i = 0; i < nv; i++) { double ax = fabs(V[3 * i]), ay = fabs(V[3 * i + 1]); if (ax > r) r = ax; if (ay > r) r = ay; }
    double sc1 = (w - 20) / (2 * r), sc2 = (h - 20) / (2 * r * sin(e_) + 10 * cos(e_)), sc = sc1 < sc2 ? sc1 : sc2;
    float *buf = malloc(sizeof(float) * 18 * (size_t)(nt ? nt : 1)); double *XYZ = malloc(sizeof(double) * 9 * (size_t)(nt ? nt : 1)); const double **UVP = malloc(sizeof(double *) * 3 * (size_t)(nt ? nt : 1)); float *sh = malloc(sizeof(float) * (nt ? nt : 1));
    if (!buf || !XYZ || !sh || !UVP) { free(buf); free(XYZ); free(sh); free(UVP); return fail(err, errlen, "out of memory", 0); }
    int n = 0; double ymin = 1e300, ymax = -1e300, zmin = 1e300, zmax = -1e300;
    for (int t = 0; t < nt; t++) {
        const double *A = V + 3 * F[3 * t], *B = V + 3 * F[3 * t + 1], *C = V + 3 * F[3 * t + 2];
        double u[3] = { B[0] - A[0], B[1] - A[1], B[2] - A[2] }, v[3] = { C[0] - A[0], C[1] - A[1], C[2] - A[2] };
        double N[3] = { u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] }, ln = sqrt(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]);
        if (!(ln > 1e-12)) continue;
        N[0] /= ln; N[1] /= ln; N[2] /= ln;
        if (!(N[0] * cam[0] + N[1] * cam[1] + N[2] * cam[2] > 1e-6)) continue;          /* faces away */
        double d = N[0] * L[0] + N[1] * L[1] + N[2] * L[2]; sh[n] = (float)(0.30 + 0.70 * (d < 0 ? 0 : d > 1 ? 1 : d));
        const double *P3[3] = { A, B, C };
        for (int k = 0; k < 3; k++) {
            const double *p = P3[k]; double X = p[0] * right[0] + p[1] * right[1] + p[2] * right[2], Y = p[0] * up[0] + p[1] * up[1] + p[2] * up[2], Z = p[0] * cam[0] + p[1] * cam[1] + p[2] * cam[2];
            XYZ[9 * n + 3 * k] = X; XYZ[9 * n + 3 * k + 1] = Y; XYZ[9 * n + 3 * k + 2] = Z; UVP[3 * n + k] = p;
            if (Y < ymin) ymin = Y; if (Y > ymax) ymax = Y; if (Z < zmin) zmin = Z; if (Z > zmax) zmax = Z;
        }
        n++;
    }
    double cy = (ymax + ymin) / 2, zr = zmax - zmin > 1e-9 ? zmax - zmin : 1.0;
    for (int t = 0; t < n; t++) for (int k = 0; k < 3; k++) {
        double X = XYZ[9 * t + 3 * k], Y = XYZ[9 * t + 3 * k + 1], Z = XYZ[9 * t + 3 * k + 2];
        double PX = w / 2.0 + X * sc, PY = h / 2.0 - (Y - cy) * sc; float *o = buf + 18 * t + 6 * k;
        o[0] = (float)(PX / w * 2.0 - 1.0); o[1] = (float)(1.0 - PY / h * 2.0); o[2] = (float)(0.01 + 0.98 * (Z - zmin) / zr); o[3] = sh[t];          /* depth: nearer = larger */
        o[4] = o[5] = -1.0f;
        if (tex) { const double *p = UVP[3 * t + k]; o[4] = (float)((p[0] - win[0]) / (win[2] - win[0])); o[5] = (float)((win[3] - p[1]) / (win[3] - win[1])); }          /* the drawing straight above: rows top down */
    }
    free(UVP);
    free(XYZ); free(sh);
    /* 2. Direct3D 11 */
    ID3D11Device *D = NULL; ID3D11DeviceContext *X = NULL; ID3D11Texture2D *ct = NULL, *dt = NULL, *st = NULL; ID3D11RenderTargetView *rtv = NULL; ID3D11DepthStencilView *dsv = NULL;
    ID3D11Buffer *vb = NULL; ID3D11VertexShader *vs = NULL; ID3D11PixelShader *ps = NULL; ID3D11InputLayout *il = NULL; ID3D11DepthStencilState *dss = NULL; ID3D11RasterizerState *rs = NULL;
    ID3D11Texture2D *tt = NULL; ID3D11ShaderResourceView *srv = NULL; ID3D11SamplerState *smp = NULL;
    ID3DBlob *bv = NULL, *bp = NULL, *be = NULL; HRESULT hr; int ok = 0; D3D_FEATURE_LEVEL fl;
    D3D_FEATURE_LEVEL want[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    int kept = KP.on && !tex && KP.D && KP.mode == mode, keep = KP.on && !tex;
    if (kept) { D = KP.D; X = KP.X; vs = KP.vs; ps = KP.ps; il = KP.il; dss = KP.dss; rs = KP.rs; snprintf(dev, devlen, "%s", KP.dev); goto ready; }
    if (keep && KP.D) shot_keep(0), KP.on = 1;          /* kept for the other mode: made again */
    hr = E_FAIL;
    if (mode == 1) hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, want, 3, D3D11_SDK_VERSION, &D, &fl, &X);
    if (FAILED(hr)) { hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, want, 3, D3D11_SDK_VERSION, &D, &fl, &X); if (FAILED(hr)) { free(buf); return fail(err, errlen, "no Direct3D 11 device", hr); } snprintf(dev, devlen, "software (WARP)"); }
    else {
        snprintf(dev, devlen, "video card");
        IDXGIDevice *gd = NULL; IDXGIAdapter *ad = NULL; DXGI_ADAPTER_DESC ds;
        if (SUCCEEDED(ID3D11Device_QueryInterface(D, &IID_IDXGIDevice, (void **)&gd)) && SUCCEEDED(IDXGIDevice_GetAdapter(gd, &ad)) && SUCCEEDED(IDXGIAdapter_GetDesc(ad, &ds))) snprintf(dev, devlen, "video card %ls", ds.Description);
        REL(ad); REL(gd);
    }
    HMODULE dc = LoadLibraryA("d3dcompiler_47.dll"); PD3DCompile comp = dc ? (PD3DCompile)(void *)GetProcAddress(dc, "D3DCompile") : NULL;
    if (!comp) { fail(err, errlen, "d3dcompiler_47.dll missing", 0); goto done; }
    if (FAILED(hr = comp(HLSL, sizeof HLSL - 1, "shot", NULL, NULL, "vs", "vs_4_0", 0, 0, &bv, &be))) { fail(err, errlen, "vertex shader", hr); goto done; }
    if (FAILED(hr = comp(HLSL, sizeof HLSL - 1, "shot", NULL, NULL, tex ? "pst" : "ps", "ps_4_0", 0, 0, &bp, &be))) { fail(err, errlen, "pixel shader", hr); goto done; }
    if (FAILED(hr = ID3D11Device_CreateVertexShader(D, ID3D10Blob_GetBufferPointer(bv), ID3D10Blob_GetBufferSize(bv), NULL, &vs))) { fail(err, errlen, "CreateVertexShader", hr); goto done; }
    if (FAILED(hr = ID3D11Device_CreatePixelShader(D, ID3D10Blob_GetBufferPointer(bp), ID3D10Blob_GetBufferSize(bp), NULL, &ps))) { fail(err, errlen, "CreatePixelShader", hr); goto done; }
    D3D11_INPUT_ELEMENT_DESC ie[3] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, { "SHADE", 0, DXGI_FORMAT_R32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                                       { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    if (FAILED(hr = ID3D11Device_CreateInputLayout(D, ie, 3, ID3D10Blob_GetBufferPointer(bv), ID3D10Blob_GetBufferSize(bv), &il))) { fail(err, errlen, "CreateInputLayout", hr); goto done; }
    {
    D3D11_DEPTH_STENCIL_DESC dd; memset(&dd, 0, sizeof dd); dd.DepthEnable = TRUE; dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; dd.DepthFunc = D3D11_COMPARISON_GREATER;
    if (FAILED(hr = ID3D11Device_CreateDepthStencilState(D, &dd, &dss))) { fail(err, errlen, "depth state", hr); goto done; }
    D3D11_RASTERIZER_DESC rd; memset(&rd, 0, sizeof rd); rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;          /* turned-away faces are already gone */
    if (FAILED(hr = ID3D11Device_CreateRasterizerState(D, &rd, &rs))) { fail(err, errlen, "rasterizer state", hr); goto done; }
    }
    if (keep) { KP.D = D; KP.X = X; KP.vs = vs; KP.ps = ps; KP.il = il; KP.dss = dss; KP.rs = rs; KP.mode = mode; snprintf(KP.dev, sizeof KP.dev, "%s", dev); kept = 1; }
ready:;
    {
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td); td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(hr = ID3D11Device_CreateTexture2D(D, &td, NULL, &ct))) { fail(err, errlen, "colour target", hr); goto done; }
    td.Format = DXGI_FORMAT_D32_FLOAT; td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(hr = ID3D11Device_CreateTexture2D(D, &td, NULL, &dt))) { fail(err, errlen, "depth target", hr); goto done; }
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(hr = ID3D11Device_CreateTexture2D(D, &td, NULL, &st))) { fail(err, errlen, "read-back texture", hr); goto done; }
    if (FAILED(hr = ID3D11Device_CreateRenderTargetView(D, (ID3D11Resource *)ct, NULL, &rtv))) { fail(err, errlen, "render target view", hr); goto done; }
    if (FAILED(hr = ID3D11Device_CreateDepthStencilView(D, (ID3D11Resource *)dt, NULL, &dsv))) { fail(err, errlen, "depth view", hr); goto done; }
    }
    if (tex) {          /* the 2D drawing as a texture, each pixel as it is (point sampling: the drawing's own colours) */
        unsigned char *t4 = malloc(4 * (size_t)tw * th); if (!t4) { fail(err, errlen, "out of memory", 0); goto done; }
        for (size_t q = 0; q < (size_t)tw * th; q++) { t4[4 * q] = tex[3 * q]; t4[4 * q + 1] = tex[3 * q + 1]; t4[4 * q + 2] = tex[3 * q + 2]; t4[4 * q + 3] = 255; }
        D3D11_TEXTURE2D_DESC xd; memset(&xd, 0, sizeof xd); xd.Width = tw; xd.Height = th; xd.MipLevels = 1; xd.ArraySize = 1; xd.SampleDesc.Count = 1;
        xd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; xd.Usage = D3D11_USAGE_IMMUTABLE; xd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA xs; memset(&xs, 0, sizeof xs); xs.pSysMem = t4; xs.SysMemPitch = 4 * tw;
        hr = ID3D11Device_CreateTexture2D(D, &xd, &xs, &tt); free(t4);
        if (FAILED(hr)) { fail(err, errlen, "colour texture", hr); goto done; }
        if (FAILED(hr = ID3D11Device_CreateShaderResourceView(D, (ID3D11Resource *)tt, NULL, &srv))) { fail(err, errlen, "colour texture view", hr); goto done; }
        D3D11_SAMPLER_DESC sd2; memset(&sd2, 0, sizeof sd2); sd2.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; sd2.AddressU = sd2.AddressV = sd2.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; sd2.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(hr = ID3D11Device_CreateSamplerState(D, &sd2, &smp))) { fail(err, errlen, "colour sampler", hr); goto done; }
    }
    if (n) {
        D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof bd); bd.ByteWidth = (UINT)(sizeof(float) * 18 * (size_t)n); bd.Usage = D3D11_USAGE_IMMUTABLE; bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd; memset(&sd, 0, sizeof sd); sd.pSysMem = buf;
        if (FAILED(hr = ID3D11Device_CreateBuffer(D, &bd, &sd, &vb))) { fail(err, errlen, "vertex buffer", hr); goto done; }
    }
    float clear[4] = { 1, 1, 1, 0 };
    ID3D11DeviceContext_ClearRenderTargetView(X, rtv, clear); ID3D11DeviceContext_ClearDepthStencilView(X, dsv, D3D11_CLEAR_DEPTH, 0.0f, 0);
    ID3D11DeviceContext_OMSetRenderTargets(X, 1, &rtv, dsv); ID3D11DeviceContext_OMSetDepthStencilState(X, dss, 0); ID3D11DeviceContext_RSSetState(X, rs);
    D3D11_VIEWPORT vp = { 0, 0, (float)w, (float)h, 0, 1 }; ID3D11DeviceContext_RSSetViewports(X, 1, &vp);
    if (n) {
        UINT stride = 24, off = 0; ID3D11DeviceContext_IASetVertexBuffers(X, 0, 1, &vb, &stride, &off); ID3D11DeviceContext_IASetInputLayout(X, il);
        ID3D11DeviceContext_IASetPrimitiveTopology(X, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext_VSSetShader(X, vs, NULL, 0); ID3D11DeviceContext_PSSetShader(X, ps, NULL, 0);
        if (tex) { ID3D11DeviceContext_PSSetShaderResources(X, 0, 1, &srv); ID3D11DeviceContext_PSSetSamplers(X, 0, 1, &smp); }
        ID3D11DeviceContext_Draw(X, (UINT)(3 * n), 0);
    }
    ID3D11DeviceContext_CopyResource(X, (ID3D11Resource *)st, (ID3D11Resource *)ct);
    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(hr = ID3D11DeviceContext_Map(X, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &ms))) { fail(err, errlen, "read back", hr); goto done; }
    for (int y = 0; y < h; y++) {
        const unsigned char *s = (const unsigned char *)ms.pData + (size_t)y * ms.RowPitch; unsigned char *o = rgb + (size_t)y * w * 3;
        for (int x = 0; x < w; x++) { if (s[4 * x + 3]) { o[3 * x] = s[4 * x]; o[3 * x + 1] = s[4 * x + 1]; o[3 * x + 2] = s[4 * x + 2]; } else o[3 * x] = o[3 * x + 1] = o[3 * x + 2] = 255; }
    }
    ID3D11DeviceContext_Unmap(X, (ID3D11Resource *)st, 0);
    ok = 1;
done:
    REL(smp); REL(srv); REL(tt); REL(be); REL(bv); REL(bp); REL(vb); REL(dsv); REL(rtv); REL(st); REL(dt); REL(ct);
    if (!kept) { REL(rs); REL(dss); REL(il); REL(ps); REL(vs); REL(X); REL(D); }          /* the kept ones stay with the thread */
    free(buf); return ok;
}
