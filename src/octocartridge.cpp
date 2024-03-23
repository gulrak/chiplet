//---------------------------------------------------------------------------------------
//
//  C++ Octo Cartridge Version with Extensions:
//
//  The MIT License (MIT)
//
//  Copyright (c) 2024, Steffen Schümann
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
//---------------------------------------------------------------------------------------
//
//  Based on original code from 'octo_cartridge.h' in C-Octo by John Earnest:
//
//  The MIT License (MIT)
//
//  Copyright (c) 2020, John Earnest
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
//---------------------------------------------------------------------------------------
#include <chiplet/utility.hpp>
#include <chiplet/octocartridge.hpp>

#define GIFIMAGE_IMPLEMENTATION
#include <chiplet/gifimage.hpp>

#include <nlohmann/json.hpp>
#include <nonstd/bit.hpp>

namespace emu {

static const char* octo_css_color_names[]={
    "alice blue","aliceblue","antique white","antiquewhite","antiquewhite1","antiquewhite2","antiquewhite3","antiquewhite4","aqua",
    "aquamarine","aquamarine1","aquamarine2","aquamarine3","aquamarine4","azure","azure1","azure2","azure3","azure4","beige","bisque",
    "bisque1","bisque2","bisque3","bisque4","black","blanched almond","blanchedalmond","blue","blue violet","blue1","blue2","blue3",
    "blue4","blueviolet","brown","brown1","brown2","brown3","brown4","burlywood","burlywood1","burlywood2","burlywood3","burlywood4",
    "cadet blue","cadetblue","cadetblue1","cadetblue2","cadetblue3","cadetblue4","chartreuse","chartreuse1","chartreuse2",
    "chartreuse3","chartreuse4","chocolate","chocolate1","chocolate2","chocolate3","chocolate4","coral","coral1","coral2","coral3",
    "coral4","cornflower blue","cornflowerblue","cornsilk","cornsilk1","cornsilk2","cornsilk3","cornsilk4","crimson","cyan","cyan1",
    "cyan2","cyan3","cyan4","dark blue","dark cyan","dark goldenrod","dark gray","dark green","dark grey","dark khaki","dark magenta",
    "dark olive green","dark orange","dark orchid","dark red","dark salmon","dark sea green","dark slate blue","dark slate gray",
    "dark slate grey","dark turquoise","dark violet","darkblue","darkcyan","darkgoldenrod","darkgoldenrod1","darkgoldenrod2",
    "darkgoldenrod3","darkgoldenrod4","darkgray","darkgreen","darkgrey","darkkhaki","darkmagenta","darkolivegreen","darkolivegreen1",
    "darkolivegreen2","darkolivegreen3","darkolivegreen4","darkorange","darkorange1","darkorange2","darkorange3","darkorange4",
    "darkorchid","darkorchid1","darkorchid2","darkorchid3","darkorchid4","darkred","darksalmon","darkseagreen","darkseagreen1",
    "darkseagreen2","darkseagreen3","darkseagreen4","darkslateblue","darkslategray","darkslategray1","darkslategray2",
    "darkslategray3","darkslategray4","darkslategrey","darkturquoise","darkviolet","deep pink","deep sky blue","deeppink","deeppink1",
    "deeppink2","deeppink3","deeppink4","deepskyblue","deepskyblue1","deepskyblue2","deepskyblue3","deepskyblue4","dim gray",
    "dim grey","dimgray","dimgrey","dodger blue","dodgerblue","dodgerblue1","dodgerblue2","dodgerblue3","dodgerblue4","firebrick",
    "firebrick1","firebrick2","firebrick3","firebrick4","floral white","floralwhite","forest green","forestgreen","fuchsia",
    "gainsboro","ghost white","ghostwhite","gold","gold1","gold2","gold3","gold4","goldenrod","goldenrod1","goldenrod2","goldenrod3",
    "goldenrod4","gray","gray0","gray1","gray10","gray100","gray11","gray12","gray13","gray14","gray15","gray16","gray17","gray18",
    "gray19","gray2","gray20","gray21","gray22","gray23","gray24","gray25","gray26","gray27","gray28","gray29","gray3","gray30",
    "gray31","gray32","gray33","gray34","gray35","gray36","gray37","gray38","gray39","gray4","gray40","gray41","gray42","gray43",
    "gray44","gray45","gray46","gray47","gray48","gray49","gray5","gray50","gray51","gray52","gray53","gray54","gray55","gray56",
    "gray57","gray58","gray59","gray6","gray60","gray61","gray62","gray63","gray64","gray65","gray66","gray67","gray68","gray69",
    "gray7","gray70","gray71","gray72","gray73","gray74","gray75","gray76","gray77","gray78","gray79","gray8","gray80","gray81",
    "gray82","gray83","gray84","gray85","gray86","gray87","gray88","gray89","gray9","gray90","gray91","gray92","gray93","gray94",
    "gray95","gray96","gray97","gray98","gray99","green","green yellow","green1","green2","green3","green4","greenyellow","grey",
    "grey0","grey1","grey10","grey100","grey11","grey12","grey13","grey14","grey15","grey16","grey17","grey18","grey19","grey2",
    "grey20","grey21","grey22","grey23","grey24","grey25","grey26","grey27","grey28","grey29","grey3","grey30","grey31","grey32",
    "grey33","grey34","grey35","grey36","grey37","grey38","grey39","grey4","grey40","grey41","grey42","grey43","grey44","grey45",
    "grey46","grey47","grey48","grey49","grey5","grey50","grey51","grey52","grey53","grey54","grey55","grey56","grey57","grey58",
    "grey59","grey6","grey60","grey61","grey62","grey63","grey64","grey65","grey66","grey67","grey68","grey69","grey7","grey70",
    "grey71","grey72","grey73","grey74","grey75","grey76","grey77","grey78","grey79","grey8","grey80","grey81","grey82","grey83",
    "grey84","grey85","grey86","grey87","grey88","grey89","grey9","grey90","grey91","grey92","grey93","grey94","grey95","grey96",
    "grey97","grey98","grey99","honeydew","honeydew1","honeydew2","honeydew3","honeydew4","hot pink","hotpink","hotpink1","hotpink2",
    "hotpink3","hotpink4","indian red","indianred","indianred1","indianred2","indianred3","indianred4","indigo","ivory","ivory1",
    "ivory2","ivory3","ivory4","khaki","khaki1","khaki2","khaki3","khaki4","lavender","lavender blush","lavenderblush",
    "lavenderblush1","lavenderblush2","lavenderblush3","lavenderblush4","lawn green","lawngreen","lemon chiffon","lemonchiffon",
    "lemonchiffon1","lemonchiffon2","lemonchiffon3","lemonchiffon4","light blue","light coral","light cyan","light goldenrod",
    "light goldenrod yellow","light gray","light green","light grey","light pink","light salmon","light sea green","light sky blue",
    "light slate blue","light slate gray","light slate grey","light steel blue","light yellow","lightblue","lightblue1","lightblue2",
    "lightblue3","lightblue4","lightcoral","lightcyan","lightcyan1","lightcyan2","lightcyan3","lightcyan4","lightgoldenrod",
    "lightgoldenrod1","lightgoldenrod2","lightgoldenrod3","lightgoldenrod4","lightgoldenrodyellow","lightgray","lightgreen",
    "lightgrey","lightpink","lightpink1","lightpink2","lightpink3","lightpink4","lightsalmon","lightsalmon1","lightsalmon2",
    "lightsalmon3","lightsalmon4","lightseagreen","lightskyblue","lightskyblue1","lightskyblue2","lightskyblue3","lightskyblue4",
    "lightslateblue","lightslategray","lightslategrey","lightsteelblue","lightsteelblue1","lightsteelblue2","lightsteelblue3",
    "lightsteelblue4","lightyellow","lightyellow1","lightyellow2","lightyellow3","lightyellow4","lime","lime green","limegreen",
    "linen","magenta","magenta1","magenta2","magenta3","magenta4","maroon","maroon1","maroon2","maroon3","maroon4",
    "medium aquamarine","medium blue","medium orchid","medium purple","medium sea green","medium slate blue","medium spring green",
    "medium turquoise","medium violet red","mediumaquamarine","mediumblue","mediumorchid","mediumorchid1","mediumorchid2",
    "mediumorchid3","mediumorchid4","mediumpurple","mediumpurple1","mediumpurple2","mediumpurple3","mediumpurple4","mediumseagreen",
    "mediumslateblue","mediumspringgreen","mediumturquoise","mediumvioletred","midnight blue","midnightblue","mint cream","mintcream",
    "misty rose","mistyrose","mistyrose1","mistyrose2","mistyrose3","mistyrose4","moccasin","navajo white","navajowhite",
    "navajowhite1","navajowhite2","navajowhite3","navajowhite4","navy","navy blue","navyblue","old lace","oldlace","olive",
    "olive drab","olivedrab","olivedrab1","olivedrab2","olivedrab3","olivedrab4","orange","orange red","orange1","orange2","orange3",
    "orange4","orangered","orangered1","orangered2","orangered3","orangered4","orchid","orchid1","orchid2","orchid3","orchid4",
    "pale goldenrod","pale green","pale turquoise","pale violet red","palegoldenrod","palegreen","palegreen1","palegreen2",
    "palegreen3","palegreen4","paleturquoise","paleturquoise1","paleturquoise2","paleturquoise3","paleturquoise4","palevioletred",
    "palevioletred1","palevioletred2","palevioletred3","palevioletred4","papaya whip","papayawhip","peach puff","peachpuff",
    "peachpuff1","peachpuff2","peachpuff3","peachpuff4","peru","pink","pink1","pink2","pink3","pink4","plum","plum1","plum2","plum3",
    "plum4","powder blue","powderblue","purple","purple1","purple2","purple3","purple4","rebecca purple","rebeccapurple","red","red1",
    "red2","red3","red4","rosy brown","rosybrown","rosybrown1","rosybrown2","rosybrown3","rosybrown4","royal blue","royalblue",
    "royalblue1","royalblue2","royalblue3","royalblue4","saddle brown","saddlebrown","salmon","salmon1","salmon2","salmon3","salmon4",
    "sandy brown","sandybrown","sea green","seagreen","seagreen1","seagreen2","seagreen3","seagreen4","seashell","seashell1",
    "seashell2","seashell3","seashell4","sienna","sienna1","sienna2","sienna3","sienna4","silver","sky blue","skyblue","skyblue1",
    "skyblue2","skyblue3","skyblue4","slate blue","slate gray","slate grey","slateblue","slateblue1","slateblue2","slateblue3",
    "slateblue4","slategray","slategray1","slategray2","slategray3","slategray4","slategrey","snow","snow1","snow2","snow3","snow4",
    "spring green","springgreen","springgreen1","springgreen2","springgreen3","springgreen4","steel blue","steelblue","steelblue1",
    "steelblue2","steelblue3","steelblue4","tan","tan1","tan2","tan3","tan4","teal","thistle","thistle1","thistle2","thistle3",
    "thistle4","tomato","tomato1","tomato2","tomato3","tomato4","turquoise","turquoise1","turquoise2","turquoise3","turquoise4",
    "violet","violet red","violetred","violetred1","violetred2","violetred3","violetred4","web gray","web green","web grey",
    "web maroon","web purple","webgray","webgreen","webgrey","webmaroon","webpurple","wheat","wheat1","wheat2","wheat3","wheat4",
    "white","white smoke","whitesmoke","x11 gray","x11 green","x11 grey","x11 maroon","x11 purple","x11gray","x11green","x11grey",
    "x11maroon","x11purple","yellow","yellow green","yellow1","yellow2","yellow3","yellow4","yellowgreen",
};

static int octo_css_color_values[]={
    0xF0F8FF,0xF0F8FF,0xFAEBD7,0xFAEBD7,0xFFEFDB,0xEEDFCC,0xCDC0B0,0x8B8378,0x00FFFF,0x7FFFD4,0x7FFFD4,0x76EEC6,0x66CDAA,0x458B74,0xF0FFFF,0xF0FFFF,
    0xE0EEEE,0xC1CDCD,0x838B8B,0xF5F5DC,0xFFE4C4,0xFFE4C4,0xEED5B7,0xCDB79E,0x8B7D6B,0x000000,0xFFEBCD,0xFFEBCD,0x0000FF,0x8A2BE2,0x0000FF,0x0000EE,
    0x0000CD,0x00008B,0x8A2BE2,0xA52A2A,0xFF4040,0xEE3B3B,0xCD3333,0x8B2323,0xDEB887,0xFFD39B,0xEEC591,0xCDAA7D,0x8B7355,0x5F9EA0,0x5F9EA0,0x98F5FF,
    0x8EE5EE,0x7AC5CD,0x53868B,0x7FFF00,0x7FFF00,0x76EE00,0x66CD00,0x458B00,0xD2691E,0xFF7F24,0xEE7621,0xCD661D,0x8B4513,0xFF7F50,0xFF7256,0xEE6A50,
    0xCD5B45,0x8B3E2F,0x6495ED,0x6495ED,0xFFF8DC,0xFFF8DC,0xEEE8CD,0xCDC8B1,0x8B8878,0xDC143C,0x00FFFF,0x00FFFF,0x00EEEE,0x00CDCD,0x008B8B,0x00008B,
    0x008B8B,0xB8860B,0xA9A9A9,0x006400,0xA9A9A9,0xBDB76B,0x8B008B,0x556B2F,0xFF8C00,0x9932CC,0x8B0000,0xE9967A,0x8FBC8F,0x483D8B,0x2F4F4F,0x2F4F4F,
    0x00CED1,0x9400D3,0x00008B,0x008B8B,0xB8860B,0xFFB90F,0xEEAD0E,0xCD950C,0x8B6508,0xA9A9A9,0x006400,0xA9A9A9,0xBDB76B,0x8B008B,0x556B2F,0xCAFF70,
    0xBCEE68,0xA2CD5A,0x6E8B3D,0xFF8C00,0xFF7F00,0xEE7600,0xCD6600,0x8B4500,0x9932CC,0xBF3EFF,0xB23AEE,0x9A32CD,0x68228B,0x8B0000,0xE9967A,0x8FBC8F,
    0xC1FFC1,0xB4EEB4,0x9BCD9B,0x698B69,0x483D8B,0x2F4F4F,0x97FFFF,0x8DEEEE,0x79CDCD,0x528B8B,0x2F4F4F,0x00CED1,0x9400D3,0xFF1493,0x00BFFF,0xFF1493,
    0xFF1493,0xEE1289,0xCD1076,0x8B0A50,0x00BFFF,0x00BFFF,0x00B2EE,0x009ACD,0x00688B,0x696969,0x696969,0x696969,0x696969,0x1E90FF,0x1E90FF,0x1E90FF,
    0x1C86EE,0x1874CD,0x104E8B,0xB22222,0xFF3030,0xEE2C2C,0xCD2626,0x8B1A1A,0xFFFAF0,0xFFFAF0,0x228B22,0x228B22,0xFF00FF,0xDCDCDC,0xF8F8FF,0xF8F8FF,
    0xFFD700,0xFFD700,0xEEC900,0xCDAD00,0x8B7500,0xDAA520,0xFFC125,0xEEB422,0xCD9B1D,0x8B6914,0xBEBEBE,0x000000,0x030303,0x1A1A1A,0xFFFFFF,0x1C1C1C,
    0x1F1F1F,0x212121,0x242424,0x262626,0x292929,0x2B2B2B,0x2E2E2E,0x303030,0x050505,0x333333,0x363636,0x383838,0x3B3B3B,0x3D3D3D,0x404040,0x424242,
    0x454545,0x474747,0x4A4A4A,0x080808,0x4D4D4D,0x4F4F4F,0x525252,0x545454,0x575757,0x595959,0x5C5C5C,0x5E5E5E,0x616161,0x636363,0x0A0A0A,0x666666,
    0x696969,0x6B6B6B,0x6E6E6E,0x707070,0x737373,0x757575,0x787878,0x7A7A7A,0x7D7D7D,0x0D0D0D,0x7F7F7F,0x828282,0x858585,0x878787,0x8A8A8A,0x8C8C8C,
    0x8F8F8F,0x919191,0x949494,0x969696,0x0F0F0F,0x999999,0x9C9C9C,0x9E9E9E,0xA1A1A1,0xA3A3A3,0xA6A6A6,0xA8A8A8,0xABABAB,0xADADAD,0xB0B0B0,0x121212,
    0xB3B3B3,0xB5B5B5,0xB8B8B8,0xBABABA,0xBDBDBD,0xBFBFBF,0xC2C2C2,0xC4C4C4,0xC7C7C7,0xC9C9C9,0x141414,0xCCCCCC,0xCFCFCF,0xD1D1D1,0xD4D4D4,0xD6D6D6,
    0xD9D9D9,0xDBDBDB,0xDEDEDE,0xE0E0E0,0xE3E3E3,0x171717,0xE5E5E5,0xE8E8E8,0xEBEBEB,0xEDEDED,0xF0F0F0,0xF2F2F2,0xF5F5F5,0xF7F7F7,0xFAFAFA,0xFCFCFC,
    0x00FF00,0xADFF2F,0x00FF00,0x00EE00,0x00CD00,0x008B00,0xADFF2F,0xBEBEBE,0x000000,0x030303,0x1A1A1A,0xFFFFFF,0x1C1C1C,0x1F1F1F,0x212121,0x242424,
    0x262626,0x292929,0x2B2B2B,0x2E2E2E,0x303030,0x050505,0x333333,0x363636,0x383838,0x3B3B3B,0x3D3D3D,0x404040,0x424242,0x454545,0x474747,0x4A4A4A,
    0x080808,0x4D4D4D,0x4F4F4F,0x525252,0x545454,0x575757,0x595959,0x5C5C5C,0x5E5E5E,0x616161,0x636363,0x0A0A0A,0x666666,0x696969,0x6B6B6B,0x6E6E6E,
    0x707070,0x737373,0x757575,0x787878,0x7A7A7A,0x7D7D7D,0x0D0D0D,0x7F7F7F,0x828282,0x858585,0x878787,0x8A8A8A,0x8C8C8C,0x8F8F8F,0x919191,0x949494,
    0x969696,0x0F0F0F,0x999999,0x9C9C9C,0x9E9E9E,0xA1A1A1,0xA3A3A3,0xA6A6A6,0xA8A8A8,0xABABAB,0xADADAD,0xB0B0B0,0x121212,0xB3B3B3,0xB5B5B5,0xB8B8B8,
    0xBABABA,0xBDBDBD,0xBFBFBF,0xC2C2C2,0xC4C4C4,0xC7C7C7,0xC9C9C9,0x141414,0xCCCCCC,0xCFCFCF,0xD1D1D1,0xD4D4D4,0xD6D6D6,0xD9D9D9,0xDBDBDB,0xDEDEDE,
    0xE0E0E0,0xE3E3E3,0x171717,0xE5E5E5,0xE8E8E8,0xEBEBEB,0xEDEDED,0xF0F0F0,0xF2F2F2,0xF5F5F5,0xF7F7F7,0xFAFAFA,0xFCFCFC,0xF0FFF0,0xF0FFF0,0xE0EEE0,
    0xC1CDC1,0x838B83,0xFF69B4,0xFF69B4,0xFF6EB4,0xEE6AA7,0xCD6090,0x8B3A62,0xCD5C5C,0xCD5C5C,0xFF6A6A,0xEE6363,0xCD5555,0x8B3A3A,0x4B0082,0xFFFFF0,
    0xFFFFF0,0xEEEEE0,0xCDCDC1,0x8B8B83,0xF0E68C,0xFFF68F,0xEEE685,0xCDC673,0x8B864E,0xE6E6FA,0xFFF0F5,0xFFF0F5,0xFFF0F5,0xEEE0E5,0xCDC1C5,0x8B8386,
    0x7CFC00,0x7CFC00,0xFFFACD,0xFFFACD,0xFFFACD,0xEEE9BF,0xCDC9A5,0x8B8970,0xADD8E6,0xF08080,0xE0FFFF,0xEEDD82,0xFAFAD2,0xD3D3D3,0x90EE90,0xD3D3D3,
    0xFFB6C1,0xFFA07A,0x20B2AA,0x87CEFA,0x8470FF,0x778899,0x778899,0xB0C4DE,0xFFFFE0,0xADD8E6,0xBFEFFF,0xB2DFEE,0x9AC0CD,0x68838B,0xF08080,0xE0FFFF,
    0xE0FFFF,0xD1EEEE,0xB4CDCD,0x7A8B8B,0xEEDD82,0xFFEC8B,0xEEDC82,0xCDBE70,0x8B814C,0xFAFAD2,0xD3D3D3,0x90EE90,0xD3D3D3,0xFFB6C1,0xFFAEB9,0xEEA2AD,
    0xCD8C95,0x8B5F65,0xFFA07A,0xFFA07A,0xEE9572,0xCD8162,0x8B5742,0x20B2AA,0x87CEFA,0xB0E2FF,0xA4D3EE,0x8DB6CD,0x607B8B,0x8470FF,0x778899,0x778899,
    0xB0C4DE,0xCAE1FF,0xBCD2EE,0xA2B5CD,0x6E7B8B,0xFFFFE0,0xFFFFE0,0xEEEED1,0xCDCDB4,0x8B8B7A,0x00FF00,0x32CD32,0x32CD32,0xFAF0E6,0xFF00FF,0xFF00FF,
    0xEE00EE,0xCD00CD,0x8B008B,0xB03060,0xFF34B3,0xEE30A7,0xCD2990,0x8B1C62,0x66CDAA,0x0000CD,0xBA55D3,0x9370DB,0x3CB371,0x7B68EE,0x00FA9A,0x48D1CC,
    0xC71585,0x66CDAA,0x0000CD,0xBA55D3,0xE066FF,0xD15FEE,0xB452CD,0x7A378B,0x9370DB,0xAB82FF,0x9F79EE,0x8968CD,0x5D478B,0x3CB371,0x7B68EE,0x00FA9A,
    0x48D1CC,0xC71585,0x191970,0x191970,0xF5FFFA,0xF5FFFA,0xFFE4E1,0xFFE4E1,0xFFE4E1,0xEED5D2,0xCDB7B5,0x8B7D7B,0xFFE4B5,0xFFDEAD,0xFFDEAD,0xFFDEAD,
    0xEECFA1,0xCDB38B,0x8B795E,0x000080,0x000080,0x000080,0xFDF5E6,0xFDF5E6,0x808000,0x6B8E23,0x6B8E23,0xC0FF3E,0xB3EE3A,0x9ACD32,0x698B22,0xFFA500,
    0xFF4500,0xFFA500,0xEE9A00,0xCD8500,0x8B5A00,0xFF4500,0xFF4500,0xEE4000,0xCD3700,0x8B2500,0xDA70D6,0xFF83FA,0xEE7AE9,0xCD69C9,0x8B4789,0xEEE8AA,
    0x98FB98,0xAFEEEE,0xDB7093,0xEEE8AA,0x98FB98,0x9AFF9A,0x90EE90,0x7CCD7C,0x548B54,0xAFEEEE,0xBBFFFF,0xAEEEEE,0x96CDCD,0x668B8B,0xDB7093,0xFF82AB,
    0xEE799F,0xCD6889,0x8B475D,0xFFEFD5,0xFFEFD5,0xFFDAB9,0xFFDAB9,0xFFDAB9,0xEECBAD,0xCDAF95,0x8B7765,0xCD853F,0xFFC0CB,0xFFB5C5,0xEEA9B8,0xCD919E,
    0x8B636C,0xDDA0DD,0xFFBBFF,0xEEAEEE,0xCD96CD,0x8B668B,0xB0E0E6,0xB0E0E6,0xA020F0,0x9B30FF,0x912CEE,0x7D26CD,0x551A8B,0x663399,0x663399,0xFF0000,
    0xFF0000,0xEE0000,0xCD0000,0x8B0000,0xBC8F8F,0xBC8F8F,0xFFC1C1,0xEEB4B4,0xCD9B9B,0x8B6969,0x4169E1,0x4169E1,0x4876FF,0x436EEE,0x3A5FCD,0x27408B,
    0x8B4513,0x8B4513,0xFA8072,0xFF8C69,0xEE8262,0xCD7054,0x8B4C39,0xF4A460,0xF4A460,0x2E8B57,0x2E8B57,0x54FF9F,0x4EEE94,0x43CD80,0x2E8B57,0xFFF5EE,
    0xFFF5EE,0xEEE5DE,0xCDC5BF,0x8B8682,0xA0522D,0xFF8247,0xEE7942,0xCD6839,0x8B4726,0xC0C0C0,0x87CEEB,0x87CEEB,0x87CEFF,0x7EC0EE,0x6CA6CD,0x4A708B,
    0x6A5ACD,0x708090,0x708090,0x6A5ACD,0x836FFF,0x7A67EE,0x6959CD,0x473C8B,0x708090,0xC6E2FF,0xB9D3EE,0x9FB6CD,0x6C7B8B,0x708090,0xFFFAFA,0xFFFAFA,
    0xEEE9E9,0xCDC9C9,0x8B8989,0x00FF7F,0x00FF7F,0x00FF7F,0x00EE76,0x00CD66,0x008B45,0x4682B4,0x4682B4,0x63B8FF,0x5CACEE,0x4F94CD,0x36648B,0xD2B48C,
    0xFFA54F,0xEE9A49,0xCD853F,0x8B5A2B,0x008080,0xD8BFD8,0xFFE1FF,0xEED2EE,0xCDB5CD,0x8B7B8B,0xFF6347,0xFF6347,0xEE5C42,0xCD4F39,0x8B3626,0x40E0D0,
    0x00F5FF,0x00E5EE,0x00C5CD,0x00868B,0xEE82EE,0xD02090,0xD02090,0xFF3E96,0xEE3A8C,0xCD3278,0x8B2252,0x808080,0x008000,0x808080,0x800000,0x800080,
    0x808080,0x008000,0x808080,0x800000,0x800080,0xF5DEB3,0xFFE7BA,0xEED8AE,0xCDBA96,0x8B7E66,0xFFFFFF,0xF5F5F5,0xF5F5F5,0xBEBEBE,0x00FF00,0xBEBEBE,
    0xB03060,0xA020F0,0xBEBEBE,0x00FF00,0xBEBEBE,0xB03060,0xA020F0,0xFFFF00,0x9ACD32,0xFFFF00,0xEEEE00,0xCDCD00,0x8B8B00,0x9ACD32,
};

uint8_t octo_cart_label_font[] = {
    // 8x5 pixels, A-Z0-9.-
    0x3F, 0x50, 0x90, 0x50, 0x3F, 0x00, 0xFF, 0x91, 0x91, 0x91, 0x6E, 0x00, 0x7E, 0x81, 0x81, 0x81, 0x42, 0x00, 0xFF, 0x81, 0x81, 0x81, 0x7E, 0x00, 0xFF, 0x91, 0x91, 0x81, 0x81, 0x00, 0xFF, 0x90, 0x90, 0x80, 0x80, 0x00, 0x7E, 0x81,
    0x91, 0x91, 0x9E, 0x00, 0xFF, 0x10, 0x10, 0x10, 0xFF, 0x00, 0x81, 0x81, 0xFF, 0x81, 0x81, 0x00, 0x02, 0x81, 0x81, 0xFE, 0x80, 0x00, 0xFF, 0x10, 0x20, 0x50, 0x8F, 0x00, 0xFF, 0x01, 0x01, 0x01, 0x01, 0x00, 0xFF, 0x40, 0x20, 0x40,
    0xFF, 0x00, 0xFF, 0x40, 0x20, 0x10, 0xFF, 0x00, 0x7E, 0x81, 0x81, 0x81, 0x7E, 0x00, 0xFF, 0x90, 0x90, 0x90, 0x60, 0x00, 0x7E, 0x81, 0x85, 0x82, 0x7D, 0x00, 0xFF, 0x90, 0x90, 0x98, 0x67, 0x00, 0x62, 0x91, 0x91, 0x91, 0x4E, 0x00,
    0x80, 0x80, 0xFF, 0x80, 0x80, 0x00, 0xFE, 0x01, 0x01, 0x01, 0xFE, 0x00, 0xFC, 0x02, 0x01, 0x02, 0xFC, 0x00, 0xFF, 0x02, 0x04, 0x02, 0xFF, 0x00, 0xC7, 0x28, 0x10, 0x28, 0xC7, 0x00, 0xC0, 0x20, 0x1F, 0x20, 0xC0, 0x00, 0x87, 0x89,
    0x91, 0xA1, 0xC1, 0x00, 0x7E, 0x81, 0x99, 0x81, 0x7E, 0x00, 0x21, 0x41, 0xFF, 0x01, 0x01, 0x00, 0x43, 0x85, 0x89, 0x91, 0x61, 0x00, 0x82, 0x81, 0xA1, 0xD1, 0x8E, 0x00, 0xF0, 0x10, 0x10, 0xFF, 0x10, 0x00, 0xF2, 0x91, 0x91, 0x91,
    0x9E, 0x00, 0x7E, 0x91, 0x91, 0x91, 0x4E, 0x00, 0x80, 0x90, 0x9F, 0xB0, 0xD0, 0x00, 0x6E, 0x91, 0x91, 0x91, 0x6E, 0x00, 0x62, 0x91, 0x91, 0x91, 0x7E, 0x00, 0x00, 0x00, 0x06, 0x06, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00,
};

uint8_t octo_cart_base_image[] = {
    0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0xA0, 0x00, 0x80, 0x00, 0xA2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x66, 0x50, 0xBF, 0xBE, 0xA6, 0xF6, 0xE3, 0x9F, 0xF6, 0xEA, 0xCF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0xF9, 0x04, 0x09, 0x00,
    0x00, 0x06, 0x00, 0x2C, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x00, 0x80, 0x00, 0x00, 0x03, 0xFF, 0x08, 0x1A, 0xDC, 0xFE, 0x30, 0xCA, 0x49, 0xAB, 0xBD, 0x38, 0x6B, 0xAC, 0xBA, 0x0F, 0x42, 0x28, 0x8E, 0x64, 0x69, 0x9E, 0x68, 0xAA, 0xAE, 0x6C, 0xEB, 0xBE, 0x6D,
    0xE0, 0x75, 0xA0, 0xD0, 0x91, 0xB7, 0x3D, 0x87, 0xF9, 0xEC, 0xFF, 0xC0, 0xA0, 0x70, 0x48, 0x2C, 0x1A, 0x8F, 0xC4, 0x90, 0xEC, 0xC3, 0x6B, 0x00, 0x44, 0x00, 0xA7, 0x00, 0x62, 0x93, 0xC2, 0xAE, 0xD8, 0xAC, 0x76, 0x7B, 0x5A, 0x2E, 0xAA, 0x20, 0xC6, 0x33,
    0x1A, 0xAE, 0x4D, 0x6B, 0xE4, 0xF3, 0x18, 0xC9, 0x6E, 0xBB, 0xDF, 0xF0, 0xA1, 0x92, 0x76, 0x16, 0x31, 0xEA, 0xA5, 0x3B, 0x7E, 0xCF, 0xED, 0xFB, 0xFF, 0x7F, 0x4B, 0x65, 0x63, 0x77, 0x62, 0x3A, 0x76, 0x35, 0x86, 0x69, 0x71, 0x8C, 0x8D, 0x8E, 0x8F, 0x3B,
    0x53, 0x51, 0x4A, 0x32, 0x7A, 0x6A, 0x96, 0x98, 0x95, 0x66, 0x80, 0x9C, 0x9D, 0x9E, 0x2C, 0x99, 0x9B, 0x67, 0x9B, 0x96, 0xA3, 0x3C, 0x90, 0xA8, 0xA9, 0xAA, 0x45, 0x7C, 0x9F, 0xAE, 0xAF, 0xB0, 0x7D, 0xA5, 0xB1, 0xB4, 0xB5, 0xB6, 0x5D, 0xA2, 0xB7, 0xBA,
    0xBB, 0xAF, 0xB3, 0xBC, 0xBF, 0xC0, 0xB2, 0xB9, 0xC1, 0xC4, 0xC5, 0x31, 0xC3, 0xC6, 0xC9, 0xCA, 0x23, 0xBE, 0xCB, 0xCE, 0xC9, 0xCD, 0xAB, 0xD2, 0xD3, 0xD4, 0x46, 0x26, 0xBE, 0x64, 0x1B, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0x86, 0x24, 0xBE, 0xE0, 0xE4,
    0xE5, 0xE6, 0xE7, 0x1A, 0x79, 0xB9, 0x0F, 0x03, 0xED, 0xEE, 0x03, 0x0C, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xEE, 0x04, 0xFE, 0xFC, 0xF4, 0x1E, 0xA8, 0x53, 0x17, 0xAF, 0x5E, 0x00, 0x80, 0x08, 0x13, 0x2A, 0x5C, 0x98, 0xCF,
    0xDF, 0x3F, 0x86, 0x0E, 0x06, 0x8A, 0x2B, 0xC8, 0xB0, 0xA2, 0xC5, 0x8B, 0x0C, 0x1D, 0x3E, 0x5C, 0xD8, 0x40, 0xFF, 0x22, 0xB3, 0x83, 0x18, 0x43, 0x8A, 0x1C, 0x59, 0x4F, 0xA3, 0xC5, 0x71, 0xEB, 0x40, 0x92, 0x5C, 0xC9, 0xF2, 0xA2, 0xC3, 0x93, 0x29, 0xD5,
    0xB5, 0x9C, 0x49, 0x33, 0xE1, 0x4B, 0x88, 0x31, 0x27, 0xD6, 0xDC, 0xC9, 0xF3, 0xDE, 0x4D, 0x85, 0x28, 0x65, 0xF6, 0x1C, 0x4A, 0x74, 0xC0, 0x4F, 0x84, 0x41, 0x75, 0x16, 0x5D, 0xCA, 0xF3, 0xE8, 0xBE, 0xA4, 0x1F, 0x99, 0x4A, 0x9D, 0x5A, 0x92, 0x00, 0x54,
    0x44, 0x54, 0xB3, 0x36, 0xD5, 0xB8, 0xD1, 0xDF, 0x55, 0x4A, 0x5A, 0xC3, 0xB6, 0xE4, 0xEA, 0xF4, 0xEB, 0x19, 0xB1, 0x68, 0x57, 0x9A, 0x9C, 0x67, 0x96, 0x62, 0xDA, 0xB7, 0x34, 0xDB, 0xAA, 0x84, 0x4B, 0x77, 0xA5, 0xDC, 0xBA, 0x78, 0x49, 0xDE, 0xCD, 0xCB,
    0xF7, 0xE2, 0xDE, 0xBE, 0x80, 0x81, 0xE6, 0x8C, 0x1A, 0xB8, 0x30, 0xC0, 0xBF, 0x86, 0x13, 0xDF, 0x43, 0xAC, 0xB8, 0xB1, 0x3C, 0xC6, 0x8E, 0x23, 0x43, 0x8E, 0xDC, 0x78, 0x32, 0xE5, 0xC4, 0x96, 0x2F, 0x17, 0xCE, 0xAC, 0x19, 0x30, 0xE7, 0xCE, 0x7C, 0x3F,
    0x83, 0xC6, 0x2B, 0x7A, 0x34, 0xDD, 0xD2, 0xA6, 0xDF, 0xA2, 0x4E, 0x8D, 0x76, 0x35, 0xEB, 0xB0, 0xAE, 0x5F, 0x67, 0x8D, 0x2D, 0x7B, 0x2A, 0xED, 0xDA, 0x4C, 0x6F, 0xE3, 0x2E, 0xAA, 0x7B, 0xF7, 0xD0, 0xDE, 0xBE, 0x79, 0x02, 0x0F, 0x5E, 0x73, 0x38, 0xF1,
    0x99, 0xC6, 0x8F, 0xB3, 0x4C, 0xAE, 0x5C, 0xEF, 0x60, 0xAC, 0xCD, 0xD3, 0x32, 0x8F, 0x1E, 0x72, 0x3A, 0x75, 0xBF, 0xCF, 0xC1, 0x5E, 0x87, 0x9D, 0xFD, 0xEC, 0x76, 0xAD, 0xD6, 0xBF, 0x73, 0xEC, 0xEE, 0x56, 0x7C, 0x6E, 0xF2, 0x73, 0xCD, 0x2F, 0x0D, 0xAF,
    0xFE, 0x30, 0xFA, 0xF6, 0x52, 0xD9, 0xC3, 0xD7, 0x27, 0x7F, 0x3E, 0xBE, 0xFA, 0xF6, 0xED, 0xE1, 0xCF, 0x1F, 0xF0, 0x3D, 0xFF, 0x9E, 0xFF, 0xFB, 0xFD, 0xF7, 0x4E, 0x80, 0x02, 0xB6, 0x43, 0x60, 0x81, 0x8C, 0xAD, 0x55, 0x20, 0x46, 0x09, 0x3A, 0xB5, 0x20,
    0x4E, 0x1E, 0x41, 0xD7, 0x8F, 0x83, 0x0F, 0x0A, 0x16, 0xA1, 0x76, 0xEF, 0x10, 0x50, 0x61, 0x75, 0xFE, 0xD9, 0xB3, 0xD1, 0x86, 0xEE, 0x5D, 0xE8, 0x9D, 0x87, 0x1A, 0x82, 0x88, 0x14, 0x7A, 0xE9, 0x35, 0x97, 0xE2, 0x6F, 0x72, 0x95, 0x77, 0x9C, 0x8B, 0x00,
    0x76, 0x34, 0x11, 0x41, 0xE5, 0xAD, 0x68, 0x9A, 0x03, 0xE7, 0xC9, 0xF8, 0x11, 0x8D, 0xE8, 0xF4, 0xE8, 0xE3, 0x8F, 0xDC, 0x88, 0x38, 0x0A, 0x90, 0x44, 0x16, 0x69, 0xA4, 0x8E, 0x3B, 0x3E, 0xA3, 0xA4, 0x32, 0xCD, 0x2C, 0xE9, 0xA4, 0x2E, 0x4D, 0x3E, 0x29,
    0x65, 0x2C, 0x51, 0x4E, 0x69, 0xA5, 0x27, 0x55, 0x5E, 0xA9, 0xA5, 0x1F, 0x59, 0x6E, 0xE9, 0x65, 0x16, 0x5D, 0x7E, 0x29, 0xA6, 0x0B, 0x61, 0x8E, 0x69, 0x66, 0x0A, 0x65, 0x9E, 0xA9, 0xE6, 0x8C, 0x6B, 0xB6, 0x79, 0x8C, 0x9B, 0x70, 0xAA, 0x90, 0x66, 0x9C,
    0x66, 0xCE, 0x49, 0xA7, 0x98, 0x76, 0xDE, 0xE9, 0x65, 0x9E, 0x7A, 0x6A, 0xC9, 0x67, 0x9F, 0x56, 0xFE, 0x09, 0xA8, 0x94, 0x82, 0x0E, 0xEA, 0x64, 0xA1, 0x86, 0x2A, 0x89, 0x68, 0xA2, 0xCE, 0x2C, 0xCA, 0x28, 0x93, 0xC8, 0x3C, 0xFA, 0xA5, 0xA3, 0x92, 0x16,
    0x43, 0x69, 0xA5, 0xC1, 0x5C, 0x8A, 0xE9, 0x2F, 0x9A, 0x6E, 0xBA, 0x4B, 0xA7, 0x9E, 0xDE, 0x02, 0x6A, 0xA8, 0xB5, 0x8C, 0x4A, 0x2A, 0x95, 0x91, 0x9E, 0x7A, 0x68, 0xAA, 0x7B, 0x4E, 0x00, 0xA7, 0xA9, 0xBB, 0x3C, 0x81, 0x82, 0xAC, 0x6B, 0xC2, 0x1A, 0x2B,
    0xAD, 0x50, 0xE0, 0xAA, 0xA6, 0xAD, 0xBC, 0x28, 0x70, 0x0A, 0x9D, 0xBC, 0xFE, 0xE2, 0xEB, 0x9D, 0x4B, 0xA8, 0xAA, 0x65, 0x24, 0xC6, 0x3E, 0x59, 0xCD, 0xB2, 0x05, 0xCC, 0x36, 0xCB, 0x46, 0x02, 0x00, 0x3B};

#if 0
static Chip8EmulatorOptions optionsFromOctoOptions(const octo_options& octo)
{
    Chip8EmulatorOptions result;
    if(octo.max_rom > 3584 || !octo.q_clip) {
        result = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eXOCHIP);
    }
    else if(octo.q_vblank || !octo.q_loadstore || !octo.q_shift) {
        result = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8);
    }
    else {
        result = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eSCHPC);
    }
    result.optJustShiftVx = octo.q_shift;
    result.optLoadStoreDontIncI = octo.q_loadstore;
    result.optLoadStoreIncIByX = false;
    result.optJump0Bxnn = octo.q_jump0;
    result.optDontResetVf = !octo.q_logic;
    result.optWrapSprites = !octo.q_clip;
    result.optInstantDxyn = !octo.q_vblank;
    result.instructionsPerFrame = octo.tickrate;
    return result;
}
#endif

//"octo","vip","dream6800","eti660","schip","fish"   {"none","swipe","seg16","seg16fill","gamepad","vip"};
// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(OctoOptions::Font, {
    {OctoOptions::Font::Octo, "octo"},
    {OctoOptions::Font::Vip, "vip"},
    {OctoOptions::Font::Dream6800, "dream6800"},
    {OctoOptions::Font::SChip, "schip"},
    {OctoOptions::Font::FishNChips, "fish"}
})
NLOHMANN_JSON_SERIALIZE_ENUM(OctoOptions::Touch, {
    {OctoOptions::Touch::None, "none"},
    {OctoOptions::Touch::Swipe, "swipe"},
    {OctoOptions::Touch::Seg16, "seg16"},
    {OctoOptions::Touch::Seg16Fill, "seg16fill"},
    {OctoOptions::Touch::GamePad, "gamepad"},
    {OctoOptions::Touch::Vip, "vip"}
})
// clang-format on

void from_json(const nlohmann::json& j, OctoOptions& opt)
{
    opt.tickrate = j.value("tickrate", opt.tickrate);
    opt.qShift = j.value("shiftQuirks", opt.qShift);
    opt.qLoadStore = j.value("loadStoreQuirks", opt.qLoadStore);
    opt.qClip = j.value("clipQuirks", opt.qClip);
    opt.qVBlank = j.value("vBlankQuirks", opt.qVBlank);
    opt.qJump0 = j.value("jumpQuirks", opt.qJump0);
    opt.qLogic = j.value("logicQuirks", opt.qLogic);
    opt.rotation = j.value("screenRotation", opt.rotation);
    if(opt.rotation != 0 && opt.rotation != 90 && opt.rotation != 180 && opt.rotation != 270)
        opt.rotation = 0;
    if(j.contains("maxSize")) {
        auto ms = j["maxSize"];
        if(ms.is_number()) {
            j.get_to(opt.maxRom);
        }
        else if(ms.is_string()) {
            opt.maxRom = std::stoul(ms.get<std::string>());
        }
    }
    if(opt.maxRom == 3216)
        opt.maxRom = 3232;
    else
        if(opt.maxRom != 3232 && opt.maxRom != 3583 && opt.maxRom != 3584 && opt.maxRom != 65024)
            opt.maxRom = 65024;
    opt.touchMode = j.value("touchInputMode", opt.touchMode);
    opt.font = j.value("fontStyle", opt.font);
}

void to_json(nlohmann::json& j, const OctoOptions& opt)
{
    j = nlohmann::json{
        {"tickrate", opt.tickrate},
        {"shiftQuirks", opt.qShift},
        {"loadStoreQuirks", opt.qLoadStore},
        {"clipQuirks", opt.qClip},
        {"vBlankQuirks", opt.qVBlank},
        {"jumpQuirks", opt.qJump0},
        {"logicQuirks", opt.qLogic},
        {"screenRotation", opt.rotation},
        {"maxSize", opt.maxRom},
        {"touchInputMode", opt.touchMode},
        {"fontStyle", opt.font},
        {"fillColor", fmt::format("#{:06x}", opt.colors[1])},
        {"fillColor2", fmt::format("#{:06x}", opt.colors[2])},
        {"blendColor", fmt::format("#{:06x}", opt.colors[3])},
        {"backgroundColor", fmt::format("#{:06x}", opt.colors[0])},
        {"buzzColor", fmt::format("#{:06x}", opt.colors[5])},
        {"quietColor", fmt::format("#{:06x}", opt.colors[4])}
    };
}

uint32_t emu::OctoCartridge::getColorFromName(const std::string& name)
{
    auto namedColors = sizeof(octo_css_color_names)/sizeof(char*);
    if(!name.empty()) {
        if(name[0] == '#') {
            return std::strtoul(name.c_str() + 1, nullptr, 16);
        }
        for (int i = 0; i < namedColors; ++i) {
            if (strcmp(octo_css_color_names[i], name.c_str()) == 0)
                return octo_css_color_values[i];
        }
    }
    return 0;
}

char OctoCartridge::getCartByte(size_t& offset) const
{
    int size = _width * _height;
    size_t index = offset % size;
    size_t frameNum = offset / size;
    if(frameNum >= _frames.size() || _frames[frameNum]._pixels.size() < size) {
        return 0;
    }
    const auto& frame = _frames[frameNum];
    auto& pal = frame._palette.empty() ? _palette : frame._palette;
    uint32_t ca = pal[frame._pixels[index] & 0xff], cb = pal[frame._pixels[index+1] & 0xff];
    uint8_t a=((ca>>13)&8)|((ca>>7)&6)|(ca&1);
    uint8_t b=((cb>>13)&8)|((cb>>7)&6)|(cb&1);
    offset += 2;
    return static_cast<char>((a<<4)|b);
}

bool OctoCartridge::loadCartridge()
{
    decodeFile(_filename);
    std::string jsonString;
    size_t offset = 0, length = 0;
    for (int z = 0; z < 4; z++) {
        length = (length << 8) | (getCartByte(offset) & 0xff);
    }
    for (int z = 0; z < length; z++)
        jsonString.push_back(getCartByte(offset));
    try {
        _jsonStr = jsonString;
        auto result = nlohmann::json::parse(jsonString);
        _options = result.value("options", _options);
        _source = result.value("program", "");
    }
    catch (...) {
    }
    return !_source.empty();
}

void OctoCartridge::printLabel(const std::string& label)
{
    // code copied verbatim from c-octo `octo_cartridge.h` by John Earnest
    // :
    // a whimsically impressionistic imitation
    // of a crummy dot-matrix printer:
    const char* alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-";
    int cx = 16, cy = 32;
    for (size_t i = 0; i < label.size(); i++) {
        char c = std::toupper(label[i]), a = std::strlen(alpha) - 1;
        for (int z = 0; z < a; z++)
            if (alpha[z] == c) {
                a = z;
                break;
            }
        if (c == ' ') {
            cx += 6;
        }
        else if (c == '\n') {
            cx = 16, cy += 9;
        }
        else {
            for (int x = 0; x < 6; x++)
                for (int y = 0; y < 8; y++) {
                    if ((x + cx > _width - 16) || (y + cy > _height) || ((rand() % 100) > 95) || (!((octo_cart_label_font[(a * 6) + x] >> (7 - y)) & 1)))
                        continue;
                    _frames.back()._pixels[(x + cx) + _width * (y + cy)] = 1;
                }
            cx += 6;
        }
        cx += (rand() % 10) > 8 ? 1 : 0;
        cy += (rand() % 10) > 8 ? 1 : 0;
    }
}

bool OctoCartridge::saveCartridge(std::string_view programSource, const std::string& label, const DataSpan& image)
{
    decode(ByteView{octo_cart_base_image, octo_cart_base_image + sizeof(octo_cart_base_image)});
    _comment = "made with Chiplet";
    if(!label.empty()){
        printLabel(label);
    }
    auto numColors = std::min(_palette.size(),16ul);
    for (int c = 0; c < numColors; c++) {
        // use 1 bit from the red/blue channels and 2 from the green channel to store data:
        for (int x = 0; x < 16; x++) {
            _palette[(16 * c) + x] = (_palette[c] & 0xFEFCFE) | ((x & 0x8) << 13) | ((x & 0x6) << 7) | (x & 1);
        }
    }
    auto json = nlohmann::json{
        {"options", _options},
        {"program", programSource}
    };
    auto jsonStr = "    " + json.dump();
    jsonStr[0] = ((jsonStr.size() - 4) >> 24) & 0xff;
    jsonStr[1] = ((jsonStr.size() - 4) >> 16) & 0xff;
    jsonStr[2] = ((jsonStr.size() - 4) >> 8) & 0xff;
    jsonStr[3] = (jsonStr.size() - 4) & 0xff;
    auto frameSize = _width * _height;
    auto frameCount = std::ceil((jsonStr.size() * 2.0) / frameSize);
    for (int z = 0; z < frameCount; ++z) {
        if (z) {
            _frames.push_back(_frames.front());
        }
        for (int i = 0; i < frameSize; i++) {
            auto src = (i + frameSize * z) / 2;                                                    // every byte in the payload becomes 2 pixels
            auto nibble = src >= jsonStr.size() ? 0 : (jsonStr[src] >> (i % 2 == 0 ? 4 : 0));         // alternate high, low nibbles
            _frames.back()._pixels[i] = ((_frames.front()._pixels[i] & 0xf) * 16) + (nibble & 0xf);  // multiply out colors and mix in data
        }
    }
    return false;
}

std::vector<uint32_t> OctoCartridge::getImage() const
{
    if(_frames.empty())
        return {};
    std::vector<uint32_t> image(_width * _height);
    const auto& frame = _frames.front();
    for(size_t y = 0; y < _height; ++y) {
        for(size_t x = 0; x < _width; ++x) {
            auto col = frame._pixels[y * _width + x];
            image[y * _width + x] = nonstd::as_big_endian(((_palette[col] << 8u) | 255u));
        }
    }
    return image;
}

const OctoOptions& OctoCartridge::getOptions() const
{
    return _options;
}

void OctoCartridge::setOptions(nlohmann::json& options)
{
    from_json(options, _options);
}

void OctoCartridge::setOptions(OctoOptions& options)
{
    _options = options;
}

const std::string& OctoCartridge::getSource() const
{
    return _source;
}

const std::string& OctoCartridge::getJsonString() const
{
    return _jsonStr;
}

}