@echo off

echo ==========================================
echo    EventBus ��ҵ����ʾ����
echo ==========================================
echo.
echo �����ʾ��չʾEventBus�����к��Ĺ���:
echo    - �̰߳�ȫ���¼�����
echo    - ��������ת��(const char* �� std::string)
echo    - �����������֧��
echo    - �������¼��ַ�
echo    - ������ͳ�Ƽ��
echo.
pause

echo.
echo [1/3] ����������ʾ
echo ----------------------
echo ���м򵥲��ԣ�չʾ�������ĺͷ�������...
echo.
simple_test.exe

echo.
echo [2/3] ʵ��Ӧ����ʾ  
echo ----------------------
echo ����ʵ��ʹ��ʾ����չʾ��ҵ��Ӧ�ó���...
echo.
usage_example.exe

echo.
echo [3/3] ����������ʾ
echo ----------------------
echo �����������ԣ��������ܺ��̰߳�ȫ����...
echo ע�⣺������Ի��������ܻ�׼�Ͷ��̲߳���
echo.
pause
complete_test.exe

echo.
echo ==========================================
echo    EventBus ��ʾ��ɣ�
echo ==========================================
echo.
echo  ��ϲ!���Ѿ�������EventBus��ȫ������:
echo.
echo    - �̰߳�ȫ�Ĳ����¼�����
echo    - ���ܵ��Զ�����ת��
echo    - �����ܵ��㿪������
echo    - ��ҵ���ļ�غ�ͳ��
echo    - ���Ķ��Ĺ���
echo.
echo  ������Ϣ��鿴��
echo    - README.md - ����ʹ���ĵ�
echo    - PROJECT_SUMMARY.md - ��Ŀ�����ܽ�
echo    - eventbus.hpp - Դ��ʵ��
echo.
echo  ����EventBus�Ѿ�׼����������������!
echo.
pause
