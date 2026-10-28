# MK360

مساعد ماوس وكيبورد USB سلكيين على Xbox 360 المعدّل، يعمل كإضافة DashLaunch
بصيغة **MK360.xex**، مع توزيع أزرار أولي يناسب Call of Duty.

**0.1.0 تجريبية — لم تُختبر على جهاز Xbox 360 حقيقي بعد.** نجح التجميع
والاختبار الآلي للنواة وفحص XEX. قبول محمّل الجهاز وUSB واستمرار التشغيل
داخل الألعاب ينتظر التجربة العملية. لا ندّعي جاهزية بديل مكافئ لـ XIM4.

- [تعليمات التركيب والأزرار والتشخيص بالعربية](docs/README_AR.md)
- [حالة الاختبارات والقيود وطريقة البناء](docs/HARDWARE_STATUS.md)
- [ملف الإعدادات](config/MK360.ini)
- [حزمة XEX التجريبية](downloads/MK360-0.1.0-experimental.zip)
- [نتائج البناء على GitHub Actions](../../actions)

النواة تدعم WASD، تحويل حركة الماوس إلى العصا اليمنى، الأزرار والزنادين،
حساسية ADS، التنعيم، تعديل الأزرار وإعدادات حسب معرّف اللعبة. F6 يوقف
التحويل، F7/F8 يغيّران الحساسية، وF9 يعيد قراءة الإعدادات.

النسخة تستهدف **17559** و**USB boot HID** فقط: ثلاثة أزرار للماوس وX/Y،
دون عجلة أو NKRO. `xbdm.xex` مطلوب افتراضياً قبل MK360 بسبب قيد معروف
في تعريف USB المرجعي، وليس مضمّناً. معالجة الفصل وإعادة التوصيل والصورة
الثابتة في ذاكرة النظام ما زالت تجريبية؛ اقرأ التعليمات قبل التجربة.

## Build

لا يتطلب البناء Microsoft XDK. سلسلة الأدوات مبنية من GCC 15.3.0 وbinutils
2.47 مع تصحيحات ABI مثبتة من [xsdk](https://github.com/xenosworks/xsdk).

```sh
bash tools/toolchain/build.sh
export PATH="$HOME/.local/mk360-xsdk/bin:$PATH"
make package
```

التبعيات وأوامر تثبيتها في وثيقة حالة الاختبارات. يبني GitHub Actions
المشروع من المصدر ويرفق ZIP وXEX والبصمات. ملف XEX إضافة نظام، ولا يُفتح
كتطبيق واجهة مستقل.

## Source and license

GPL-3.0-only. USB integration is derived from public hiddriver360 research.
The adapted xsdk packer, import generator and compiler patches retain their
upstream notices. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and LICENSE.
No XDK headers, proprietary libraries or Microsoft tool binaries are included.
