# crontab-rsync
리눅스 환경에서 crontab과 rsync 기능 구현

- ssu_crontab
  사용자가 주기적으로 실행하는 명령어를 'ssu_crontab_file'에 저장 및 삭제하는 프로그램
  add <실행주기> <명령어>
  remove <COMMAND_NUMBER>
  exit
  세 가지 명령어 수행
  
- ssu_crond
  주기적으로 'ssu_crontab_file'에 저장된 명령어를 실행시키는 디몬 프로그램
  정상적으로 수행된 명령어를 'ssu_crontab_log' 파일에 로그 기록
  
- ssu_rsync
  명령어 형태 : ssu_rsync (src file) (dst file)
  인자로 주어진 소스파일 혹은 디렉토리를 dst 디렉토리에 동기화
  동기화 작업 중 SIGINT가 발생하면 동기화 작업 취소
  동기화 완료 시 'ssu_rsync_log' 파일에 로그 기록
