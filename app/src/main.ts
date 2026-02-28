import { createSSRApp } from "vue";
import App from "./App.vue";
export function createApp() {
  // uni-app 约定导出工厂函数，便于不同端在各自上下文里创建实例。
  const app = createSSRApp(App);
  return {
    app,
  };
}
