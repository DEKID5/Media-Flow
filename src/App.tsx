/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { ReactNode } from 'react';
import { OperatorDashboard } from './components/OperatorDashboard';
import { AudienceView } from './components/AudienceView';

interface Props {
  children: ReactNode;
}

interface State {
  hasError: boolean;
  errorMessage: string;
}

class ErrorBoundary extends React.Component<Props, State> {
  state: State = { hasError: false, errorMessage: '' };

  static getDerivedStateFromError(error: Error): State {
    return { hasError: true, errorMessage: error.message };
  }

  componentDidCatch(error: Error, errorInfo: React.ErrorInfo) {
    console.error("App Crash:", error, errorInfo);
  }

  render() {
    if (this.state.hasError) {
      return (
        <div className="h-screen w-screen bg-black flex flex-col items-center justify-center p-12 text-center text-white font-sans">
          <h1 className="text-2xl font-black mb-4 uppercase tracking-widest text-red-500">System Error</h1>
          <p className="text-white/60 mb-8 max-w-lg font-mono text-xs">{this.state.errorMessage || 'An unexpected error occurred in the views.'}</p>
          <button 
            onClick={() => window.location.reload()}
            className="px-8 py-3 bg-red-600/20 text-red-500 border border-red-500/50 rounded flex items-center justify-center font-bold uppercase tracking-widest text-xs hover:bg-red-600/40 transition-all cursor-pointer"
          >
            Restart Engine
          </button>
        </div>
      );
    }
    return (this as any).props.children;
  }
}

export default function App() {
  const searchParams = new URLSearchParams(window.location.search);
  const view = searchParams.get('view');

  return (
    <ErrorBoundary>
      {view === 'audience' || view === 'timer' ? <AudienceView /> : <OperatorDashboard />}
    </ErrorBoundary>
  );
}
