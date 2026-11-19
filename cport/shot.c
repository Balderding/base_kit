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
    "struct VI { float3 p : POSITION; float s : SHADE; };\n"
    "struct VO { float4 p : SV_Position; nointerpolation float s : SHADE; };\n"
    "VO vs(VI i) { VO o; o.p = float4(i.p, 1.0); o.s = i.s; return o; }\n"
    "float4 ps(VO i) : SV_Target { return float4(i.s * 0.86, i.s * 0.84, i.s * 0.80, 1.0); }\n";

#define REL(x) do { if (x) { IUnknown_Release((IUnknown *)(x)); (x) = NULL; } } while (0)

static int fail(char *err, int n, const char *what, HRESULT hr) { snprintf(err, n, "%s (0x%08lx)", what, (unsigned long)hr); return 0; }

int shot_render(const double *V, int nv, const int *F, int nt, int w, int h, double elev, double azim, int mode,
                unsigned char *rgb, char *dev, int devlen, char *err, int errlen) {
    if (w < 8 || h < 8 || w > 16384 || h > 16384) return fail(err, errlen, "picture size", 0);
    /* 1. the triangles on the screen, exactly as mesher.shot works them out */
    double e_ = elev * M_PI / 180.0, a_ = azim * M_PI / 180.0;
    double cam[3] = { cos(e_) * cos(a_), cos(e_) * sin(a_), sin(e_) }, right[3] = { -sin(a_), cos(a_), 0.0 }, up[3];
    up[0] = cam[1] * right[2] - cam[2] * right[1]; up[1] = cam[2] * right[0] - cam[0] * right[2]; up[2] = cam[0] * right[1] - cam[1] * right[0];
    double L[3] = { -0.45, -0.35, 0.82 }, ll = sqrt(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]); L[0] /= ll; L[1] /= ll; L[2] /= ll;
    double r = 1.0; for (int i = 0; i < nv; i++) { double ax = fabs(V[3 * i]), ay = fabs(V[3 * i + 1]); if (ax > r) r = ax; if (ay > r) r = ay; }
    double sc1 = (w - 20) / (2 * r), sc2 = (h - 20) / (2 * r * sin(e_) + 10 * cos(e_)), sc = sc1 < sc2 ? sc1 : sc2;
    float *buf = malloc(sizeof(float) * 12 * (size_t)(nt ? nt : 1)); double *XYZ = malloc(sizeof(double) * 9 * (size_t)(nt ? nt : 1)); float *sh = malloc(sizeof(float) * (nt ? nt : 1));
    if (!buf || !XYZ || !sh) { free(buf); free(XYZ); free(sh); return fail(err, errlen, "out of memory", 0); }
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
            XYZ[9 * n + 3 * k] = X; XYZ[9 * n + 3 * k + 1] = Y; XYZ[9 * n + 3 * k + 2] = Z;
            if (Y < ymin) ymin = Y; if (Y > ymax) ymax = Y; if (Z < zmin) zmin = Z; if (Z > zmax) zmax = Z;
        }
        n++;
    }
    double cy = (ymax + ymin) / 2, zr = zmax - zmin > 1e-9 ? zmax - zmin : 1.0;
    for (int t = 0; t < n; t++) for (int k = 0; k < 3; k++) {
        double X = XYZ[9 * t + 3 * k], Y = XYZ[9 * t + 3 * k + 1], Z = XYZ[9 * t + 3 * k + 2];
        double PX = w / 2.0 + X * sc, PY = h / 2.0 - (Y - cy) * sc; float *o = buf + 12 * t + 4 * k;
        o[0] = (float)(PX / w * 2.0 - 1.0); o[1] = (float)(1.0 - PY / h * 2.0); o[2] = (float)(0.01 + 0.98 * (Z - zmin) / zr); o[3] = sh[t];          /* depth: nearer = larger */
    }
    free(XYZ); free(sh);
    /* 2. Direct3D 11 */
    ID3D11Device *D = NULL; ID3D11DeviceContext *X = NULL; ID3D11Texture2D *ct = NULL, *dt = NULL, *st = NULL; ID3D11RenderTargetView *rtv = NULL; ID3D11DepthStencilView *dsv = NULL;
    ID3D11Buffer *vb = NULL; ID3D11VertexShader *vs = NULL; ID3D11PixelShader *ps = NULL; ID3D11InputLayout *il = NULL; ID3D11DepthStencilState *dss = NULL; ID3D11RasterizerState *rs = NULL;
    ID3DBlob *bv = NULL, *bp = NULL, *be = NULL; HRESULT hr; int ok = 0; D3D_FEATURE_LEVEL fl;
    D3D_FEATURE_LEVEL want[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
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
    if (FAILED(hr = comp(HLSL, sizeof HLSL - 1, "shot", NULL, NULL, "ps", "ps_4_0", 0, 0, &bp, &be))) { fail(err, errlen, "pixel shader", hr); goto done; }
    if (FAILED(hr = ID3D11Device_CreateVertexShader(D, ID3D10Blob_GetBufferPointer(bv), ID3D10Blob_GetBufferSize(bv), NULL, &vs))) { fail(err, errlen, "CreateVertexShader", hr); goto done; }
    if (FAILED(hr = ID3D11Device_CreatePixelShader(D, ID3D10Blob_GetBufferPointer(bp), ID3D10Blob_GetBufferSize(bp), NULL, &ps))) { fail(err, errlen, "CreatePixelShader", hr); goto done; }
    D3D11_INPUT_ELEMENT_DESC ie[2] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, { "SHADE", 0, DXGI_FORMAT_R32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    if (FAILED(hr = ID3D11Device_CreateInputLayout(D, ie, 2, ID3D10Blob_GetBufferPointer(bv), ID3D10Blob_GetBufferSize(bv), &il))) { fail(err, errlen, "CreateInputLayout", hr); goto done; }
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td); td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(hr = ID3D11Device_CreateTexture2D(D, &td, NULL, &ct))) { fail(err, errlen, "colour target", hr); goto done; }
    td.Format = DXGI_FORMAT_D32_FLOAT; td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(hr = ID3D11Device_CreateTexture2D(D, &td, NULL, &dt))) { fail(err, errlen, "depth target", hr); goto done; }
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(hr = ID3D11Device_CreateTexture2D(D, &td, NULL, &st))) { fail(err, errlen, "read-back texture", hr); goto done; }
    if (FAILED(hr = ID3D11Device_CreateRenderTargetView(D, (ID3D11Resource *)ct, NULL, &rtv))) { fail(err, errlen, "render target view", hr); goto done; }
    if (FAILED(hr = ID3D11Device_CreateDepthStencilView(D, (ID3D11Resource *)dt, NULL, &dsv))) { fail(err, errlen, "depth view", hr); goto done; }
    D3D11_DEPTH_STENCIL_DESC dd; memset(&dd, 0, sizeof dd); dd.DepthEnable = TRUE; dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; dd.DepthFunc = D3D11_COMPARISON_GREATER;
    if (FAILED(hr = ID3D11Device_CreateDepthStencilState(D, &dd, &dss))) { fail(err, errlen, "depth state", hr); goto done; }
    D3D11_RASTERIZER_DESC rd; memset(&rd, 0, sizeof rd); rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;          /* turned-away faces are already gone */
    if (FAILED(hr = ID3D11Device_CreateRasterizerState(D, &rd, &rs))) { fail(err, errlen, "rasterizer state", hr); goto done; }
    if (n) {
        D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof bd); bd.ByteWidth = (UINT)(sizeof(float) * 12 * (size_t)n); bd.Usage = D3D11_USAGE_IMMUTABLE; bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd; memset(&sd, 0, sizeof sd); sd.pSysMem = buf;
        if (FAILED(hr = ID3D11Device_CreateBuffer(D, &bd, &sd, &vb))) { fail(err, errlen, "vertex buffer", hr); goto done; }
    }
    float clear[4] = { 1, 1, 1, 0 };
    ID3D11DeviceContext_ClearRenderTargetView(X, rtv, clear); ID3D11DeviceContext_ClearDepthStencilView(X, dsv, D3D11_CLEAR_DEPTH, 0.0f, 0);
    ID3D11DeviceContext_OMSetRenderTargets(X, 1, &rtv, dsv); ID3D11DeviceContext_OMSetDepthStencilState(X, dss, 0); ID3D11DeviceContext_RSSetState(X, rs);
    D3D11_VIEWPORT vp = { 0, 0, (float)w, (float)h, 0, 1 }; ID3D11DeviceContext_RSSetViewports(X, 1, &vp);
    if (n) {
        UINT stride = 16, off = 0; ID3D11DeviceContext_IASetVertexBuffers(X, 0, 1, &vb, &stride, &off); ID3D11DeviceContext_IASetInputLayout(X, il);
        ID3D11DeviceContext_IASetPrimitiveTopology(X, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext_VSSetShader(X, vs, NULL, 0); ID3D11DeviceContext_PSSetShader(X, ps, NULL, 0);
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
    REL(be); REL(bv); REL(bp); REL(rs); REL(dss); REL(il); REL(ps); REL(vs); REL(vb); REL(dsv); REL(rtv); REL(st); REL(dt); REL(ct); REL(X); REL(D);
    free(buf); return ok;
}
